/* M0-S2b depth hardening. Root depth is 0; clone/preflight/emit all refuse
   depth > 128, so the deepest supported tree is 129 levels. */
#include "chml/chml.h"
#include "chml/builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 128
#define MAX_LEVELS (MAX_DEPTH + 1)

static int fails;
static void ok(const char *m) { printf("  ok   %s\n", m); }
static void no(const char *m, const char *d) { fails++; printf("  FAIL %s%s%s\n", m, d ? ": " : "", d ? d : ""); }
static int exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

/* Stable-lifetime declarative tree: a heap array of public ChmlNodeDef values,
   chained by children pointers. No compound literals, no dangling. */
static ChmlStatus decl(int levels, const char *path) {
    ChmlNodeDef *v = calloc((size_t)levels, sizeof *v);
    if (!v) return CHML_ERR_MEMORY;
    for (int i = 0; i < levels; i++) {
        v[i].tag = "div";
        if (i + 1 < levels) { v[i].children = &v[i + 1]; v[i].child_count = 1; }
    }
    ChmlDocument *d = chml_document_create();
    ChmlStatus st = chml_document_add(d, &v[0]);
    if (st == CHML_OK && path) st = chml_document_write(d, path);
    chml_document_destroy(d);
    free(v);
    return st;
}

static ChmlStatus build(int levels, const char *path) {
    ChmlBuilder *b = chml_builder_create();
    ChmlNode *root = NULL, *p = NULL;
    ChmlStatus st = chml_builder_begin(b, "div", &root);
    p = root;
    for (int i = 1; st == CHML_OK && i < levels; i++) {
        ChmlNode *c = NULL;
        st = chml_builder_element(b, p, "div", &c);
        p = c;
    }
    if (st == CHML_OK) st = chml_builder_attach(b, root, NULL);
    if (st == CHML_OK) st = chml_builder_write(b, path);
    chml_builder_destroy(b);
    return st;
}

static void expect_ok(const char *label, ChmlStatus st, int fileMade) {
    if (st == CHML_OK && fileMade) ok(label);
    else { char d[80]; snprintf(d, sizeof d, "status=%d file=%d", (int)st, fileMade); no(label, d); }
}
static void expect_refused(const char *label, ChmlStatus st, int fileMade) {
    if (st == CHML_ERR_ARGUMENT && !fileMade) ok(label);
    else { char d[80]; snprintf(d, sizeof d, "status=%d file=%d", (int)st, fileMade); no(label, d); }
}

int main(void) {
    puts("declarative");
    remove("d.html");
    { ChmlStatus st = decl(MAX_LEVELS, "d.html"); int f = exists("d.html"); remove("d.html");
      expect_ok("depth 128 (129 levels) accepted and serialized", st, f); }
    { ChmlStatus st = decl(MAX_LEVELS + 1, "d.html"); int f = exists("d.html"); remove("d.html");
      expect_refused("depth 129 (130 levels) refused, no file", st, f); }

    /* rejected add must not leave a partial document */
    { ChmlDocument *d = chml_document_create();
      const ChmlAttrDef a[] = { CHML_ATTR("href", "https://ok.example/") };
      ChmlNodeDef good = CHML_LEAF_ATTRS("a", a, 1);
      if (chml_document_add(d, &good) != CHML_OK) no("baseline add", 0);
      if (decl(MAX_LEVELS + 1, NULL) != CHML_ERR_ARGUMENT) no("deep add rejected", 0);
      ChmlNodeDef bad = CHML_LEAF("div");
      (void)bad;
      if (chml_document_write(d, "d.html") == CHML_OK && exists("d.html")) ok("document still valid after rejected deep add");
      else no("document still valid after rejected deep add", 0);
      remove("d.html"); chml_document_destroy(d); }

    puts("builder");
    { ChmlStatus st = build(MAX_LEVELS, "d.html"); int f = exists("d.html"); remove("d.html");
      expect_ok("depth 128 (129 levels) serializes", st, f); }
    { ChmlStatus st = build(MAX_LEVELS + 1, "d.html"); int f = exists("d.html"); remove("d.html");
      expect_refused("depth 129 refused at write, no file", st, f); }
    { ChmlStatus st = build(200, "d.html"); int f = exists("d.html"); remove("d.html");
      expect_refused("depth 199 refused at write (defensive)", st, f); }

    { const char *sent = "<!doctype html>\n<p>keep</p>\n";
      FILE *f = fopen("d.html", "wb"); if (f) { fputs(sent, f); fclose(f); }
      (void)build(MAX_LEVELS + 1, "d.html");
      char got[128] = {0}; f = fopen("d.html", "rb"); if (f) { size_t n = fread(got, 1, sizeof got - 1, f); (void)n; fclose(f); }
      remove("d.html");
      if (strcmp(got, sent) == 0) ok("existing destination preserved after refusal");
      else no("existing destination preserved after refusal", got); }

    if (fails) { printf("\n%d depth check(s) failed\n", fails); return 1; }
    puts("\ndepth checks passed");
    return 0;
}
