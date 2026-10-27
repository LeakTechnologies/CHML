/* M0-S2b URL policy tests. Every negative case is exercised through both public
   write APIs and asserts three independent conditions: the policy status, that
   no destination is created, and that an existing destination is untouched. */
#include "chml/chml.h"
#include "chml/builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void ok(const char *label) { printf("  ok   %s\n", label); }
static void bad(const char *label, const char *detail) {
    failures++;
    printf("  FAIL %s%s%s\n", label, detail ? ": " : "", detail ? detail : "");
}

static long slurp(const char *path, char *buf, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return (long)n;
}

/* Direct (declarative) write. Returns the status. */
static ChmlStatus via_document(const char *elem, const char *attr, const char *value,
                               int boolean_attr, const char *path) {
    ChmlDocument *d = chml_document_create();
    if (!d) return CHML_ERR_MEMORY;
    ChmlAttrDef a[1];
    a[0].name = attr;
    a[0].value = boolean_attr ? NULL : value;
    a[0].kind = boolean_attr ? CHML_ATTR_BOOLEAN : CHML_ATTR_STRING;
    ChmlNodeDef n = CHML_LEAF_ATTRS(elem, a, 1);
    ChmlStatus st = chml_document_add(d, &n);
    if (st == CHML_OK) st = chml_document_write(d, path);
    chml_document_destroy(d);
    return st;
}

/* Builder write: begin -> attribute -> attach -> write. */
static ChmlStatus via_builder(const char *elem, const char *attr, const char *value,
                              int boolean_attr, const char *path) {
    ChmlBuilder *b = chml_builder_create();
    if (!b) return CHML_ERR_MEMORY;
    ChmlNode *root = NULL;
    ChmlStatus st = chml_builder_begin(b, elem, &root);
    if (st == CHML_OK) {
        st = boolean_attr ? chml_builder_attr_bool(b, root, attr)
                          : chml_builder_attr_string(b, root, attr, value);
    }
    if (st == CHML_OK) st = chml_builder_attach(b, root, NULL);
    if (st == CHML_OK) st = chml_builder_write(b, path);
    chml_builder_destroy(b);
    return st;
}

/* Negative case: policy status, no file created, existing file preserved. */
static void expect_refused(const char *label, const char *elem, const char *attr,
                           const char *value) {
    char path[128];
    snprintf(path, sizeof path, "pol_neg_%d.html", failures + (int)strlen(label));

    remove(path);
    ChmlStatus a = via_document(elem, attr, value, 0, path);
    int created = 0;
    { FILE *f = fopen(path, "rb"); if (f) { created = 1; fclose(f); } remove(path); }

    ChmlStatus b = via_builder(elem, attr, value, 0, path);
    int created_b = 0;
    { FILE *f = fopen(path, "rb"); if (f) { created_b = 1; fclose(f); } }

    /* existing destination must survive a refusal */
    const char *sentinel = "<!doctype html>\n<p>previous</p>\n";
    FILE *f = fopen(path, "wb"); if (f) { fputs(sentinel, f); fclose(f); }
    ChmlStatus c = via_document(elem, attr, value, 0, path);
    char got[256] = {0};
    slurp(path, got, sizeof got);
    int preserved = (strcmp(got, sentinel) == 0);
    remove(path);

    int good = (a == CHML_ERR_POLICY) && (b == CHML_ERR_POLICY) &&
               !created && !created_b && (c == CHML_ERR_POLICY) && preserved;
    if (good) ok(label);
    else {
        char detail[200];
        snprintf(detail, sizeof detail,
                 "doc=%d builder=%d created=%d/%d preserved=%d",
                 (int)a, (int)b, created, created_b, preserved);
        bad(label, detail);
    }
}

/* Positive case: accepted and serialized with the expected content. */
static void expect_ok(const char *label, const char *elem, const char *attr,
                      const char *value, const char *needle) {
    char path[128];
    snprintf(path, sizeof path, "pol_pos_%d.html", (int)strlen(label) + failures);
    remove(path);
    ChmlStatus a = via_document(elem, attr, value, 0, path);
    char got[512] = {0};
    slurp(path, got, sizeof got);
    remove(path);
    ChmlStatus b = via_builder(elem, attr, value, 0, path);
    remove(path);
    int good = (a == CHML_OK) && (b == CHML_OK) && (!needle || strstr(got, needle));
    if (good) ok(label);
    else {
        char detail[220];
        snprintf(detail, sizeof detail, "doc=%d builder=%d content=%.150s",
                 (int)a, (int)b, got);
        bad(label, detail);
    }
}

int main(void) {
    puts("P1 schemes");
    expect_ok("https allowed", "a", "href", "https://example.com/x", "href=\"https://example.com/x\"");
    expect_ok("mixed-case HTTPS allowed", "a", "href", "HTTPS://example.com/x", NULL);
    expect_ok("http allowed", "a", "href", "http://example.com/", NULL);
    expect_ok("mailto on a[href]", "a", "href", "mailto:x@example.com", NULL);
    expect_ok("tel on a[href]", "a", "href", "tel:+15551234", NULL);
    expect_refused("javascript refused", "a", "href", "javascript:alert(1)");
    expect_refused("mixed-case javascript refused", "a", "href", "JaVaScRiPt:alert(1)");
    expect_refused("vbscript refused", "a", "href", "vbscript:x");
    expect_refused("data refused", "a", "href", "data:text/html,x");
    expect_refused("blob refused", "a", "href", "blob:https://x/y");
    expect_refused("file refused", "a", "href", "file:///etc/passwd");
    expect_refused("mailto refused on src", "img", "src", "mailto:x@example.com");

    puts("P2 relative references");
    expect_ok("root relative", "a", "href", "/a/b", NULL);
    expect_ok("current-dir with colon", "a", "href", "./a:b", NULL);
    expect_ok("root relative with colon", "a", "href", "/a:b", NULL);
    expect_ok("parent-dir", "a", "href", "../a", NULL);
    expect_ok("fragment", "a", "href", "#frag", NULL);
    expect_ok("query", "a", "href", "?q=1", NULL);
    expect_ok("path relative", "a", "href", "images/a.png", NULL);
    expect_refused("protocol-relative refused", "a", "href", "//evil.example/x");
    expect_refused("unsupported scheme a:b/c", "a", "href", "a:b/c");

    puts("P3 character rejection");
    expect_refused("leading space", "a", "href", " javascript:alert(1)");
    expect_refused("embedded tab", "a", "href", "java\tscript:x");
    expect_refused("backslash before colon", "a", "href", "https://x\\@y");
    expect_refused("backslash beyond byte 32", "a", "href",
                   "https://example.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\path");

    puts("P4 attribute contexts");
    expect_refused("src javascript", "img", "src", "javascript:x");
    expect_refused("action javascript", "form", "action", "javascript:x");
    expect_refused("formaction javascript", "button", "formaction", "javascript:x");
    expect_refused("poster javascript", "video", "poster", "javascript:x");
    expect_ok("action https", "form", "action", "https://example.com/s", NULL);

    puts("P5 empty values");
    expect_ok("a[href] empty allowed", "a", "href", "", "href=\"\"");
    expect_ok("form[action] empty allowed", "form", "action", "", "action=\"\"");
    expect_refused("link[href] empty refused", "link", "href", "");
    expect_refused("img[src] empty refused", "img", "src", "");
    expect_refused("formaction empty refused", "button", "formaction", "");

    puts("P6 foreign content");
    /* xlink:href is designated in SVG but ignored in HTML: this distinguishes
       namespace-aware handling from a flat attribute-name lookup. */
    {
        ChmlDocument *d = chml_document_create();
        const ChmlAttrDef a[] = {CHML_ATTR("xlink:href", "javascript:x")};
        ChmlNodeDef A = CHML_LEAF_ATTRS("a", a, 1);
        ChmlNodeDef svg = CHML_ELEMENT("svg", A);
        ChmlStatus st = chml_document_add(d, &svg);
        if (st == CHML_OK) st = chml_document_write(d, "pol_svg.html");
        remove("pol_svg.html");
        chml_document_destroy(d);
        if (st == CHML_ERR_POLICY) ok("SVG a[xlink:href] javascript refused");
        else bad("SVG a[xlink:href] javascript refused", "not refused");
    }
    {
        ChmlDocument *d = chml_document_create();
        const ChmlAttrDef a[] = {CHML_ATTR("xlink:href", "javascript:x")};
        ChmlNodeDef A = CHML_LEAF_ATTRS("a", a, 1);
        ChmlStatus st = chml_document_add(d, &A);
        if (st == CHML_OK) st = chml_document_write(d, "pol_html.html");
        int created = 0; { FILE *f = fopen("pol_html.html", "rb"); if (f) { created = 1; fclose(f); } }
        remove("pol_html.html");
        chml_document_destroy(d);
        if (st == CHML_OK && created) ok("HTML a[xlink:href] ignored (outside matrix)");
        else bad("HTML a[xlink:href] ignored (outside matrix)", "unexpected refusal");
    }
    {
        ChmlDocument *d = chml_document_create();
        const ChmlAttrDef a[] = {CHML_ATTR("href", "https://ok.example/x")};
        ChmlNodeDef A = CHML_LEAF_ATTRS("a", a, 1);
        ChmlNodeDef math = CHML_ELEMENT("math", A);
        ChmlStatus st = chml_document_add(d, &math);
        if (st == CHML_OK) st = chml_document_write(d, "pol_math.html");
        remove("pol_math.html");
        chml_document_destroy(d);
        if (st == CHML_ERR_POLICY) ok("MathML fails closed even for https");
        else bad("MathML fails closed even for https", "not refused");
    }
    {
        ChmlDocument *d = chml_document_create();
        const ChmlAttrDef a[] = {CHML_ATTR("href", "javascript:x")};
        ChmlNodeDef A = CHML_LEAF_ATTRS("a", a, 1);
        ChmlNodeDef fo = CHML_ELEMENT("foreignObject", A);
        ChmlNodeDef svg = CHML_ELEMENT("svg", fo);
        ChmlStatus st = chml_document_add(d, &svg);
        if (st == CHML_OK) st = chml_document_write(d, "pol_fo.html");
        remove("pol_fo.html");
        chml_document_destroy(d);
        if (st == CHML_ERR_POLICY) ok("foreignObject children take HTML policy");
        else bad("foreignObject children take HTML policy", "not refused");
    }

    puts("P7 prohibited attributes");
    expect_refused("srcset refused", "img", "srcset", "a.png 1x, javascript:x 2x");
    expect_refused("srcset empty refused", "img", "srcset", "");
    expect_refused("srcdoc refused", "iframe", "srcdoc", "<script>x</script>");
    expect_refused("srcdoc empty refused", "iframe", "srcdoc", "");
    expect_refused("object[data] refused", "object", "data", "https://example.com/x");

    puts("P8 serialization");
    expect_ok("entity-looking value escaped, not executed", "a", "href",
              "&#106;avascript:alert(1)", "href=\"&amp;#106;avascript:alert(1)\"");

    puts("P9 atomicity");
    {
        char path[] = "pol_atomic.html";
        remove(path);
        ChmlStatus st = via_document("a", "href", "javascript:x", 0, path);
        int created = 0; { FILE *f = fopen(path, "rb"); if (f) { created = 1; fclose(f); } remove(path); }
        if (st == CHML_ERR_POLICY && !created) ok("refusal creates no destination");
        else bad("refusal creates no destination", "file was created");

        const char *sentinel = "<!doctype html>\n<p>keep</p>\n";
        FILE *f = fopen(path, "wb"); if (f) { fputs(sentinel, f); fclose(f); }
        st = via_builder("a", "href", "javascript:x", 0, path);
        char got[256] = {0}; slurp(path, got, sizeof got);
        remove(path);
        if (st == CHML_ERR_POLICY && strcmp(got, sentinel) == 0) ok("existing destination preserved (builder)");
        else bad("existing destination preserved (builder)", got);
    }

    if (failures) { printf("\n%d policy check(s) failed\n", failures); return 1; }
    puts("\npolicy checks passed");
    return 0;
}
