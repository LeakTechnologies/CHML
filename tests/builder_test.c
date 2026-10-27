/* M0.5-A builder tests: detached construction, attachment, rollback, discard,
   deterministic allocation-failure injection, and parity with the declarative
   path so the two front-ends cannot diverge. */
#include "chml/builder.h"
#include "chml/chml.h"
#include "chml_builder_internal.h"
#include "components.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int cond, const char *msg) {
    if (cond) printf("  ok   %s\n", msg);
    else { failures++; printf("  FAIL %s\n", msg); }
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return NULL; }
    size_t got = fread(s, 1, (size_t)n, f);
    fclose(f);
    s[got] = 0;
    return s;
}

typedef struct { int calls, fail_at; } FailCtx;
static void *f_alloc(void *ctx, size_t n) {
    FailCtx *c = (FailCtx *)ctx;
    c->calls++;
    if (c->fail_at && c->calls == c->fail_at) return NULL;
    return malloc(n);
}
static void f_free(void *ctx, void *p) { (void)ctx; free(p); }

static int count_of(const char *hay, const char *needle) {
    int n = 0; size_t len = strlen(needle);
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += len) n++;
    return n;
}

/* ---- 1. exact output of a small builder page ---- */
static void test_small_page(void) {
    ChmlBuilder *b = chml_builder_create();
    ChmlNode *html = NULL, *body = NULL, *p = NULL;
    ChmlStatus st = chml_builder_begin(b, "html", &html);
    if (st == CHML_OK) st = chml_builder_attr_string(b, html, "lang", "en");
    if (st == CHML_OK) st = chml_builder_element(b, html, "body", &body);
    if (st == CHML_OK) st = chml_builder_element(b, body, "p", &p);
    if (st == CHML_OK) st = chml_builder_text(b, p, "A & B < C");
    if (st == CHML_OK) st = chml_builder_attach(b, html, NULL);
    if (st == CHML_OK) st = chml_builder_write(b, "builder_small.html");
    char *got = read_all("builder_small.html");
    const char *want = "<!doctype html>\n<html lang=\"en\"><body><p>A &amp; B &lt; C</p></body></html>\n";
    check(st == CHML_OK && got && strcmp(got, want) == 0, "small page serializes exactly");
    free(got); remove("builder_small.html");
    chml_builder_destroy(b);
}

/* ---- 2. parity with the declarative path ---- */
static void test_declarative_parity(void) {
    const ChmlAttrDef attrs[] = { CHML_ATTR("class", "x") };
    ChmlDocument *d = chml_document_create();
    ChmlNodeDef page = CHML_WITH_ATTRS("div", attrs, CHML_COUNT(attrs),
        CHML_ELEMENT("p", CHML_TEXT("A & B < C")),
        CHML_LEAF_ATTRS("input", (const ChmlAttrDef[]){ CHML_BOOLEAN_ATTR("disabled") }, 1));
    if (chml_document_add(d, &page) != CHML_OK ||
        chml_document_write(d, "parity_decl.html") != CHML_OK) {
        check(0, "declarative parity page built");
        chml_document_destroy(d);
        return;
    }
    chml_document_destroy(d);

    ChmlBuilder *b = chml_builder_create();
    ChmlNode *div = NULL, *p = NULL, *in = NULL;
    ChmlStatus st = chml_builder_begin(b, "div", &div);
    if (st == CHML_OK) st = chml_builder_attr_string(b, div, "class", "x");
    if (st == CHML_OK) st = chml_builder_element(b, div, "p", &p);
    if (st == CHML_OK) st = chml_builder_text(b, p, "A & B < C");
    if (st == CHML_OK) st = chml_builder_element(b, div, "input", &in);
    if (st == CHML_OK) st = chml_builder_attr_bool(b, in, "disabled");
    if (st == CHML_OK) st = chml_builder_attach(b, div, NULL);
    if (st == CHML_OK) st = chml_builder_write(b, "parity_build.html");

    char *a = read_all("parity_decl.html");
    char *c = read_all("parity_build.html");
    check(st == CHML_OK && a && c && strcmp(a, c) == 0,
          "builder output is byte-identical to the declarative path");
    free(a); free(c);
    remove("parity_decl.html"); remove("parity_build.html");
    chml_builder_destroy(b);
}

/* ---- 3. discard reclaims and leaves the document untouched ---- */
static void test_discard(void) {
    ChmlBuilder *b = chml_builder_create();
    ChmlNode *root = NULL;
    chml_builder_begin(b, "p", &root);
    chml_builder_text(b, root, "kept");
    chml_builder_attach(b, root, NULL);
    chml_builder_write(b, "discard.html");
    size_t baseline = chml_builder_test_bytes(b);

    ChmlNode *junk = NULL;
    chml_builder_begin(b, "div", &junk);
    for (int i = 0; i < 50; i++) chml_builder_text(b, junk, "discarded payload");
    size_t during = chml_builder_test_bytes(b);
    chml_builder_discard(b, junk);

    check(during > baseline, "discarded session allocated memory");
    check(chml_builder_test_bytes(b) == baseline, "discard releases all session memory");

    chml_builder_write(b, "discard2.html");
    char *a = read_all("discard.html");
    char *c = read_all("discard2.html");
    check(a && c && strcmp(a, c) == 0, "discard does not modify the committed document");
    free(a); free(c);
    remove("discard.html"); remove("discard2.html");
    chml_builder_destroy(b);
}

/* ---- 4. per-component rollback ---- */
static void test_rollback(void) {
    ChmlBuilder *b = chml_builder_create();
    /* Block size 1 so every allocation is observable; otherwise the temporary
       subtree would fit inside an existing block and reclamation would be
       indistinguishable from doing nothing. */
    chml_builder_test_set_block_size(b, 1);
    ChmlNode *main = NULL;
    page_layout_begin(b, "T", &main);
    size_t before = chml_builder_test_bytes(b);

    ChmlMark mark = chml_builder_mark(b, main);
    ChmlNode *card = NULL;
    chml_builder_element(b, main, "article", &card);
    chml_builder_add_text(b, card, "h2", "temporary");
    chml_builder_element(b, card, "p", NULL);
    size_t during = chml_builder_test_bytes(b);
    chml_builder_rollback(b, &mark);

    check(during > before, "the temporary subtree allocated memory");
    check(chml_builder_test_bytes(b) == before, "rollback reclaims the session memory");

    ChmlNode *probe = NULL;
    ChmlStatus st = chml_builder_element(b, main, "section", &probe);
    check(st == CHML_OK, "parent accepts new children after rollback");
    page_layout_commit(b);
    chml_builder_write(b, "rollback.html");

    char *got = read_all("rollback.html");
    check(got && strstr(got, "<main class=\"shell\"><section></section></main>") != NULL,
          "rolled-back subtree is absent from the output");
    check(got && count_of(got, "<article") == 0, "rolled-back article does not appear");
    free(got); remove("rollback.html");
    chml_builder_destroy(b);
}

/* ---- 5. deterministic allocation failure at every point ---- */
static void test_failure_injection(void) {
    int points = 0, clean = 0, last = 0, atomic_ok = 0;
    for (int n = 1; n <= 40; n++) {
        FailCtx ctx = { 0, 0 };
        ChmlBuilder *b = chml_builder_create();
        chml_builder_test_set_allocator(b, f_alloc, f_free, &ctx);
        /* Block size 1 makes every node and string allocation addressable;
           otherwise the card would fit inside an existing 4 KiB block and no
           allocator call would happen at all. */
        chml_builder_test_set_block_size(b, 1);

        ChmlNode *root = NULL;
        chml_builder_begin(b, "main", &root);
        chml_builder_add_text(b, root, "p", "committed");
        chml_builder_attach(b, root, NULL);
        size_t baseline = chml_builder_test_bytes(b);

        int base = ctx.calls;
        ctx.fail_at = base + n;          /* failure window starts at the card */

        /* Construction is session-scoped: build detached, then attach. */
        ChmlNode *card = NULL;
        ChmlStatus st = chml_builder_begin(b, "article", &card);
        if (st == CHML_OK) st = chml_builder_add_text(b, card, "h2", "Card");
        if (st == CHML_OK) st = chml_builder_add_text(b, card, "p", "Body");
        if (st == CHML_OK) st = chml_builder_element(b, card, "p", NULL);
        if (st == CHML_OK) st = chml_builder_attach(b, card, root);
        ctx.fail_at = 0;

        if (st != CHML_OK) {
            points++;
            last = n;
            chml_builder_discard(b, card);
            size_t after = chml_builder_test_bytes(b);
            if (after == baseline) clean++;

            /* Attachment atomicity: a failure anywhere in the attach path must
               leave the committed document byte-identical. All validation and
               allocation happen before the ownership splice; this asserts it. */
            if (chml_builder_write(b, "fi_doc.html") != CHML_OK) {
                printf("       n=%d: write after failed attach failed\n", n);
            } else {
                char *doc = read_all("fi_doc.html");
                const char *want = "<!doctype html>\n<main><p>committed</p></main>\n";
                if (!doc || strcmp(doc, want) != 0) {
                    printf("       n=%d: document changed after failed attach\n", n);
                } else atomic_ok++;
                free(doc);
                remove("fi_doc.html");
            }

        } else {
            chml_builder_destroy(b);
            break;                        /* n exceeded the card's allocation count */
        }
        chml_builder_destroy(b);
    }
    check(points >= 5, "allocation failure injected at several distinct points");
    check(clean == points, "every failed construction rolls back to the baseline");
    check(atomic_ok == points, "failed attachment leaves the document byte-identical");
    printf("       (%d failure points, last at allocation %d)\n", points, last);
}

/* ---- 6. ownership boundaries ---- */
static void test_ownership(void) {
    ChmlBuilder *a = chml_builder_create();
    ChmlBuilder *other = chml_builder_create();

    ChmlNode *root = NULL;
    chml_builder_begin(a, "main", &root);
    chml_builder_attach(a, root, NULL);

    ChmlNode *foreign = NULL;
    chml_builder_begin(other, "aside", &foreign);

    check(chml_builder_element(a, foreign, "p", NULL) == CHML_ERR_ARGUMENT,
          "a handle from another builder is rejected");
    check(chml_builder_attach(other, foreign, root) == CHML_ERR_ARGUMENT,
          "attaching a foreign subtree to this document is rejected");

    /* attaching a session node as parent is invalid */
    ChmlNode *inner = NULL;
    chml_builder_element(other, foreign, "div", &inner);
    check(chml_builder_attach(other, foreign, inner) == CHML_ERR_ARGUMENT,
          "a detached node cannot receive a committed subtree");

    chml_builder_discard(other, foreign);
    chml_builder_destroy(other);
    chml_builder_destroy(a);
}

/* ---- 7. stable handles after attachment and later mutation ---- */
static void test_stable_handles(void) {
    ChmlBuilder *b = chml_builder_create();
    ChmlNode *main = NULL;
    page_layout_begin(b, "T", &main);
    ChmlNode *card = NULL;
    chml_builder_element(b, main, "article", &card);
    chml_builder_add_text(b, card, "h2", "stable");
    page_layout_commit(b);

    ChmlNode *saved = card;
    ChmlNode *extra = NULL;
    chml_builder_begin(b, "footer", &extra);
    chml_builder_attach(b, extra, NULL);          /* mutate after the handle existed */

    check(saved == card, "committed handle keeps its address");
    chml_builder_write(b, "stable.html");
    char *got = read_all("stable.html");
    check(got && strstr(got, "<article><h2>stable</h2></article>") != NULL,
          "committed handle still refers to the same node after mutation");
    free(got); remove("stable.html");
    chml_builder_destroy(b);
}

/* ---- 8. the reference example ---- */
static void test_example(void) {
    static const ChmlNavLink links[] = { { "Projects", "#projects" }, { "About", "#about" } };
    ChmlBuilder *b = chml_builder_create();
    ChmlNode *main = NULL;
    ChmlStatus st = page_layout_begin(b, "Leak Technologies", &main);
    if (st == CHML_OK) st = navigation_add(b, main, links, 2);
    if (st == CHML_OK) st = project_card_add(b, main, "VideoTools", "Media & <processing>", "#v");
    if (st == CHML_OK) st = project_card_add(b, main, "CHML", "The web, written in C.", "#c");
    if (st == CHML_OK) st = page_layout_commit(b);
    if (st == CHML_OK) st = chml_builder_write(b, "example.html");

    char *got = read_all("example.html");
    check(st == CHML_OK && got, "reference example builds");
    if (got) {
        check(count_of(got, "<nav") == 1 && count_of(got, "<li>") == 2,
              "navigation is not wrapped twice");
        check(count_of(got, "<article") == 2, "two card instances");
        check(count_of(got, "<main") == 1, "no accidental main wrapper");
        check(strstr(got, "Media &amp; &lt;processing&gt;") != NULL,
              "card text is escaped exactly as the declarative path does");
        check(strstr(got, "<meta charset=\"utf-8\">") != NULL,
              "charset declaration is injected by the shared writer");
    }
    free(got); remove("example.html");
    chml_builder_destroy(b);
}

int main(void) {
    test_small_page();
    test_declarative_parity();
    test_discard();
    test_rollback();
    test_failure_injection();
    test_ownership();
    test_stable_handles();
    test_example();
    if (failures) { printf("%d builder check(s) failed\n", failures); return 1; }
    puts("builder checks passed");
    return 0;
}
