/* Transactional component builder.
   Construction happens in a detached session region; the document region is
   only touched by chml_builder_attach. A failed or abandoned session is freed
   wholesale, so the committed document cannot be left partially modified. */
#include "chml/builder.h"
#include "chml_builder_internal.h"
#include "chml_node.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct Block {
    struct Block *next;
    size_t used, cap;
    unsigned char data[1];
} Block;

typedef struct {
    Block *head;
    size_t block_size;
} Region;

struct ChmlBuilder {
    Region doc;
    Region session;
    int session_open;
    struct ChmlNode *session_root;
    struct ChmlNode **roots;
    size_t root_count, root_cap;
    ChmlTestAlloc alloc;
    ChmlTestFree release;
    void *alloc_ctx;
    size_t bytes;
};

static void *default_alloc(void *ctx, size_t n) { (void)ctx; return malloc(n); }
static void  default_free (void *ctx, void *p)   { (void)ctx; free(p); }

static size_t align8(size_t n) { return (n + 7u) & ~(size_t)7u; }

static void *region_alloc(ChmlBuilder *b, Region *r, size_t n) {
    n = align8(n);
    if (r->head && r->head->cap - r->head->used >= n) {
        void *p = r->head->data + r->head->used;
        r->head->used += n;
        return p;
    }
    size_t cap = r->block_size > n ? r->block_size : n;
    Block *blk = b->alloc(b->alloc_ctx, sizeof(Block) + cap);
    if (!blk) return NULL;
    blk->next = r->head; blk->used = n; blk->cap = cap;
    r->head = blk;
    b->bytes += sizeof(Block) + cap;
    return blk->data;
}
static void region_free(ChmlBuilder *b, Region *r) {
    Block *blk = r->head;
    while (blk) {
        Block *next = blk->next;
        b->bytes -= sizeof(Block) + blk->cap;
        b->release(b->alloc_ctx, blk);
        blk = next;
    }
    r->head = NULL;
}
/* Moves every block of src into dst. Pure pointer work; cannot fail. */
static void region_splice(ChmlBuilder *b, Region *dst, Region *src) {
    (void)b;
    if (!src->head) return;
    Block *tail = src->head;
    while (tail->next) tail = tail->next;
    tail->next = dst->head;
    dst->head = src->head;
    src->head = NULL;
}
static int region_contains(const Region *r, const void *p) {
    uintptr_t v = (uintptr_t)p;
    for (const Block *blk = r->head; blk; blk = blk->next) {
        uintptr_t lo = (uintptr_t)blk->data;
        uintptr_t hi = lo + blk->used;
        if (v >= lo && v < hi) return 1;
    }
    return 0;
}

static char *region_strdup(ChmlBuilder *b, Region *r, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = region_alloc(b, r, n);
    if (p) memcpy(p, s, n);
    return p;
}

static ChmlStatus append_child(ChmlBuilder *b, Region *r, struct ChmlNode *parent,
                               struct ChmlNode *child) {
    if (parent->child_count == parent->child_cap) {
        size_t cap = parent->child_cap ? parent->child_cap * 2 : 4;
        struct ChmlNode **arr = region_alloc(b, r, cap * sizeof(*arr));
        if (!arr) return CHML_ERR_MEMORY;
        if (parent->child_count)
            memcpy(arr, parent->children, parent->child_count * sizeof(*arr));
        parent->children = arr;
        parent->child_cap = cap;
    }
    parent->children[parent->child_count++] = child;
    return CHML_OK;
}

static struct ChmlNode *node_new(ChmlBuilder *b, Region *r, const char *tag,
                                 const char *text) {
    struct ChmlNode *n = region_alloc(b, r, sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    if (tag) {
        n->tag = region_strdup(b, r, tag);
        if (!n->tag) return NULL;
    }
    if (text) {
        n->text = region_strdup(b, r, text);
        if (!n->text) return NULL;
    }
    return n;
}

/* Construction happens only inside the open session. Committed document nodes
   accept content through chml_builder_attach, never by direct mutation, so a
   rollback can never strand unreclaimable blocks in the document region. */
static Region *session_of(ChmlBuilder *b, struct ChmlNode *n) {
    if (b->session_open && region_contains(&b->session, n)) return &b->session;
    return NULL;
}

ChmlBuilder *chml_builder_create(void) {
    ChmlBuilder *b = malloc(sizeof(*b));
    if (!b) return NULL;
    memset(b, 0, sizeof(*b));
    b->doc.block_size = 4096;
    b->session.block_size = 4096;
    b->alloc = default_alloc;
    b->release = default_free;
    return b;
}
void chml_builder_destroy(ChmlBuilder *b) {
    if (!b) return;
    region_free(b, &b->session);
    region_free(b, &b->doc);
    free(b->roots);
    free(b);
}
void chml_builder_test_set_allocator(ChmlBuilder *b, ChmlTestAlloc alloc,
                                     ChmlTestFree release, void *ctx) {
    if (!b) return;
    b->alloc = alloc ? alloc : default_alloc;
    b->release = release ? release : default_free;
    b->alloc_ctx = ctx;
}
size_t chml_builder_test_bytes(const ChmlBuilder *b) { return b ? b->bytes : 0; }
void chml_builder_test_set_block_size(ChmlBuilder *b, size_t block_size) {
    if (!b || !block_size) return;
    b->doc.block_size = block_size;
    b->session.block_size = block_size;
}

/* Frees blocks allocated after a checkpoint and restores the bump pointer. */
static void region_rewind(ChmlBuilder *b, Region *r, Block *keep, size_t used) {
    while (r->head && r->head != keep) {
        Block *next = r->head->next;
        b->bytes -= sizeof(Block) + r->head->cap;
        b->release(b->alloc_ctx, r->head);
        r->head = next;
    }
    if (keep) keep->used = used;
}

ChmlStatus chml_builder_begin(ChmlBuilder *b, const char *tag, ChmlNode **root) {
    if (!b || !root || b->session_open) return CHML_ERR_ARGUMENT;
    if (!chml_name_valid(tag)) return CHML_ERR_ARGUMENT;
    /* A failure here must not leave blocks in a session that never opens,
       because the caller has no handle with which to discard them. */
    Block *keep = b->session.head;
    size_t used = keep ? keep->used : 0;
    struct ChmlNode *n = node_new(b, &b->session, tag, NULL);
    if (!n) {
        region_rewind(b, &b->session, keep, used);
        return CHML_ERR_MEMORY;
    }
    b->session_open = 1;
    b->session_root = n;
    *root = n;
    return CHML_OK;
}

ChmlStatus chml_builder_element(ChmlBuilder *b, ChmlNode *parent, const char *tag,
                                ChmlNode **out) {
    if (!b || !parent || !chml_name_valid(tag)) return CHML_ERR_ARGUMENT;
    Region *r = session_of(b, parent);
    if (!r) return CHML_ERR_ARGUMENT;          /* foreign or discarded handle */
    struct ChmlNode *n = node_new(b, r, tag, NULL);
    if (!n) return CHML_ERR_MEMORY;
    ChmlStatus st = append_child(b, r, parent, n);
    if (st != CHML_OK) return st;
    if (out) *out = n;
    return CHML_OK;
}

ChmlStatus chml_builder_text(ChmlBuilder *b, ChmlNode *parent, const char *text) {
    if (!b || !parent || !text) return CHML_ERR_ARGUMENT;
    Region *r = session_of(b, parent);
    if (!r) return CHML_ERR_ARGUMENT;
    struct ChmlNode *n = node_new(b, r, NULL, text);
    if (!n) return CHML_ERR_MEMORY;
    return append_child(b, r, parent, n);
}

ChmlStatus chml_builder_add_text(ChmlBuilder *b, ChmlNode *parent, const char *tag,
                                 const char *text) {
    struct ChmlNode *n = NULL;
    ChmlStatus st = chml_builder_element(b, parent, tag, &n);
    if (st != CHML_OK) return st;
    return chml_builder_text(b, n, text);
}

static ChmlStatus attr_push(ChmlBuilder *b, Region *r, struct ChmlNode *node,
                            const ChmlAttrDef *def) {
    if (node->attr_count == node->attr_cap) {
        size_t cap = node->attr_cap ? node->attr_cap * 2 : 4;
        ChmlAttrDef *arr = region_alloc(b, r, cap * sizeof(*arr));
        if (!arr) return CHML_ERR_MEMORY;
        if (node->attr_count)
            memcpy(arr, node->attrs, node->attr_count * sizeof(*arr));
        node->attrs = arr;
        node->attr_cap = cap;
    }
    ChmlAttrDef *slot = &node->attrs[node->attr_count];
    slot->name = region_strdup(b, r, def->name);
    slot->value = def->value ? region_strdup(b, r, def->value) : NULL;
    slot->kind = def->kind;
    if (!slot->name || (def->value && !slot->value)) return CHML_ERR_MEMORY;
    node->attr_count++;
    return CHML_OK;
}

ChmlStatus chml_builder_attr_string(ChmlBuilder *b, ChmlNode *node,
                                    const char *name, const char *value) {
    if (!b || !node) return CHML_ERR_ARGUMENT;
    Region *r = session_of(b, node);
    if (!r) return CHML_ERR_ARGUMENT;
    ChmlAttrDef def = { name, value, CHML_ATTR_STRING };
    if (chml_attr_validate(&def) != CHML_OK) return CHML_ERR_ARGUMENT;
    for (size_t i = 0; i < node->attr_count; i++) {
        /* Duplicate names are rejected by the declarative path too. */
        const char *a = node->attrs[i].name, *c = name;
        int same = 1;
        while (*a && *c) {
            unsigned char ca = (unsigned char)*a++, cc = (unsigned char)*c++;
            if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
            if (cc >= 'A' && cc <= 'Z') cc = (unsigned char)(cc - 'A' + 'a');
            if (ca != cc) { same = 0; break; }
        }
        if (same && !*a && !*c) return CHML_ERR_ARGUMENT;
    }
    return attr_push(b, r, node, &def);
}

ChmlStatus chml_builder_attr_bool(ChmlBuilder *b, ChmlNode *node, const char *name) {
    if (!b || !node) return CHML_ERR_ARGUMENT;
    Region *r = session_of(b, node);
    if (!r) return CHML_ERR_ARGUMENT;
    ChmlAttrDef def = { name, NULL, CHML_ATTR_BOOLEAN };
    if (chml_attr_validate(&def) != CHML_OK) return CHML_ERR_ARGUMENT;
    for (size_t i = 0; i < node->attr_count; i++)
        if (node->attrs[i].name && strcmp(node->attrs[i].name, name) == 0)
            return CHML_ERR_ARGUMENT;
    return attr_push(b, r, node, &def);
}

ChmlMark chml_builder_mark(ChmlBuilder *b, ChmlNode *parent) {
    ChmlMark m;
    memset(&m, 0, sizeof(m));
    if (!b || !b->session_open) return m;
    m.block = b->session.head;
    m.used = b->session.head ? b->session.head->used : 0;
    m.parent = parent;
    if (parent) {
        m.children = parent->children;
        m.child_count = parent->child_count;
        m.child_cap = parent->child_cap;
    }
    m.valid = 1;
    return m;
}

void chml_builder_rollback(ChmlBuilder *b, ChmlMark *mark) {
    if (!b || !mark || !mark->valid || !b->session_open) return;
    region_rewind(b, &b->session, (Block *)mark->block, mark->used);
    if (mark->parent) {
        mark->parent->children = mark->children;
        mark->parent->child_count = mark->child_count;
        mark->parent->child_cap = mark->child_cap;
    }
    mark->valid = 0;
}

ChmlStatus chml_builder_attach(ChmlBuilder *b, ChmlNode *root, ChmlNode *parent) {
    if (!b) return CHML_ERR_ARGUMENT;
    if (!b->session_open) return CHML_ERR_ARGUMENT;
    if (!root) root = b->session_root;      /* convenience: attach the session root */
    if (root != b->session_root) return CHML_ERR_ARGUMENT;
    if (parent) {
        Region *pr = region_contains(&b->doc, parent) ? &b->doc : NULL;
        /* Only document-owned nodes may receive a committed subtree. */
        if (!pr || pr == &b->session) return CHML_ERR_ARGUMENT;
    }

    if (parent) {
        ChmlStatus st = append_child(b, &b->doc, parent, root);
        if (st != CHML_OK) return st;
    } else {
        if (b->root_count == b->root_cap) {
            size_t cap = b->root_cap ? b->root_cap * 2 : 4;
            struct ChmlNode **arr = realloc(b->roots, cap * sizeof(*arr));
            if (!arr) return CHML_ERR_MEMORY;
            b->roots = arr;
            b->root_cap = cap;
        }
        b->roots[b->root_count++] = root;
    }
    /* Ownership transfer. Cannot fail; must immediately follow the link. */
    region_splice(b, &b->doc, &b->session);
    b->session_open = 0;
    b->session_root = NULL;
    return CHML_OK;
}

void chml_builder_discard(ChmlBuilder *b, ChmlNode *root) {
    if (!b || !root || !b->session_open || root != b->session_root) return;
    region_free(b, &b->session);
    b->session_open = 0;
    b->session_root = NULL;
}

ChmlStatus chml_builder_write(const ChmlBuilder *b, const char *path) {
    if (!b || !path) return CHML_ERR_ARGUMENT;
    return chml_write_node_list(b->roots, b->root_count, path);
}
