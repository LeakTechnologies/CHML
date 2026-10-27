/* M0.5-B: component-authoring ergonomics investigation.
   Three ways to write the same failing-capable component over the existing
   detached-session builder. No library changes: every candidate lives here so
   the public API is not changed by an experiment.

   Each approach is scored by how many places must remember to roll back, and
   proven correct by failing at every allocation point. */
#include "chml/builder.h"
#include "chml_builder_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int cond, const char *msg) {
    if (cond) printf("  ok   %s\n", msg);
    else { failures++; printf("  FAIL %s\n", msg); }
}

typedef struct { int calls, fail_at; } FailCtx;
static void *f_alloc(void *ctx, size_t n) {
    FailCtx *c = (FailCtx *)ctx;
    c->calls++;
    if (c->fail_at && c->calls == c->fail_at) return NULL;
    return malloc(n);
}
static void f_free(void *ctx, void *p) { (void)ctx; free(p); }

/* ---------------- A. single exit with goto cleanup ---------------- */
/* Cleanup sites: 1. */
static ChmlStatus card_goto(ChmlBuilder *b, ChmlNode *parent) {
    ChmlMark mark = chml_builder_mark(b, parent);
    ChmlNode *card = NULL, *p = NULL;
    ChmlStatus st = chml_builder_element(b, parent, "article", &card);
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "h2", "Card");
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "p", "Body");
    if (st == CHML_OK) st = chml_builder_element(b, card, "p", &p);
    if (st == CHML_OK) st = chml_builder_text(b, p, "Read more");
    if (st != CHML_OK) {
        chml_builder_rollback(b, &mark);
        return st;
    }
    return CHML_OK;
}

/* ---------------- B. structured macro ---------------- */
/* Cleanup sites: 0 in the component, but the macro captures `b` and `mark`
   from the enclosing scope, which is invisible at the call site. */
#define CHML_TRY_ROLLBACK(expr)                                         \
    do {                                                                \
        ChmlStatus s_ = (expr);                                         \
        if (s_ != CHML_OK) { chml_builder_rollback(b, &mark); return s_; } \
    } while (0)

static ChmlStatus card_macro(ChmlBuilder *b, ChmlNode *parent) {
    ChmlMark mark = chml_builder_mark(b, parent);
    ChmlNode *card = NULL, *p = NULL;
    CHML_TRY_ROLLBACK(chml_builder_element(b, parent, "article", &card));
    CHML_TRY_ROLLBACK(chml_builder_add_text(b, card, "h2", "Card"));
    CHML_TRY_ROLLBACK(chml_builder_add_text(b, card, "p", "Body"));
    CHML_TRY_ROLLBACK(chml_builder_element(b, card, "p", &p));
    CHML_TRY_ROLLBACK(chml_builder_text(b, p, "Read more"));
    return CHML_OK;
}

/* ---------------- C. scoped callback ---------------- */
/* Cleanup sites: 0. Ownership of the transaction moves into the helper; the
   body cannot forget to roll back because it never sees the mark. */
typedef ChmlStatus (*ChmlComponentBody)(ChmlBuilder *b, ChmlNode *parent, void *ctx);

static ChmlStatus chml_component_scope(ChmlBuilder *b, ChmlNode *parent,
                                       ChmlComponentBody body, void *ctx) {
    ChmlMark mark = chml_builder_mark(b, parent);
    ChmlStatus st = body(b, parent, ctx);
    if (st != CHML_OK) chml_builder_rollback(b, &mark);
    return st;
}

static ChmlStatus card_body(ChmlBuilder *b, ChmlNode *parent, void *ctx) {
    (void)ctx;
    ChmlNode *card = NULL, *p = NULL;
    ChmlStatus st = chml_builder_element(b, parent, "article", &card);
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "h2", "Card");
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "p", "Body");
    if (st == CHML_OK) st = chml_builder_element(b, card, "p", &p);
    if (st == CHML_OK) st = chml_builder_text(b, p, "Read more");
    return st;
}
static ChmlStatus card_scope(ChmlBuilder *b, ChmlNode *parent) {
    return chml_component_scope(b, parent, card_body, NULL);
}

/* ---------------- harness ---------------- */
typedef ChmlStatus (*CardFn)(ChmlBuilder *, ChmlNode *);

static void run_pattern(const char *name, CardFn fn, int cleanup_sites) {
    int points = 0, clean = 0, succeeded = 0;
    for (int n = 1; n <= 30; n++) {
        FailCtx ctx = { 0, 0 };
        ChmlBuilder *b = chml_builder_create();
        chml_builder_test_set_allocator(b, f_alloc, f_free, &ctx);
        chml_builder_test_set_block_size(b, 1);

        ChmlNode *root = NULL;
        chml_builder_begin(b, "main", &root);
        chml_builder_add_text(b, root, "p", "committed");
        chml_builder_attach(b, root, NULL);
        size_t baseline = chml_builder_test_bytes(b);

        int base = ctx.calls;
        ctx.fail_at = base + n;

        ChmlNode *card = NULL;
        ChmlStatus st = chml_builder_begin(b, "section", &card);
        if (st == CHML_OK) st = chml_builder_element(b, card, "div", NULL);
        if (st == CHML_OK) st = fn(b, card);
        ctx.fail_at = 0;

        if (st == CHML_OK) { succeeded++; chml_builder_discard(b, card); }
        else {
            points++;
            chml_builder_discard(b, card);
            if (chml_builder_test_bytes(b) == baseline) clean++;
        }
        chml_builder_destroy(b);
        if (succeeded && n > 1) break;
    }
    printf("  %-28s cleanup sites=%d  failure points=%d  reclaimed=%d\n",
           name, cleanup_sites, points, clean);
    check(points >= 3, name);
    check(clean == points, "  reclaimed at every failure point");
}

int main(void) {
    puts("M0.5-B ergonomics comparison");
    run_pattern("A. goto cleanup", card_goto, 1);
    run_pattern("B. structured macro", card_macro, 0);
    run_pattern("C. scoped callback", card_scope, 0);

    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    puts("ergonomics checks passed");
    return 0;
}
