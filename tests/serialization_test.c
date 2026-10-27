/* Independent HTML serialization fixtures for M0.4-A.
   Expected output is written out in full rather than derived from the writer,
   so a regression in the serializer cannot also move the expectation. */
#include "chml/chml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREFIX "<!doctype html>\n"
static int failures = 0;

static char *read_all(const char *path, size_t *len) {
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
    if (len) *len = got;
    return s;
}

static void fail(const char *label, const char *got, const char *want) {
    failures++;
    printf("  FAIL %s\n    got : %s\n    want: %s\n", label,
           got ? got : "(null)", want ? want : "(null)");
}

/* Add, write, and compare the whole file against PREFIX + body + "\n". */
static void expect_body(const char *label, const ChmlNodeDef *def, const char *body) {
    ChmlDocument *d = chml_document_create();
    if (!d) { failures++; return; }
    ChmlStatus st = chml_document_add(d, def);
    if (st != CHML_OK) {
        failures++;
        printf("  FAIL %s: add returned %d\n", label, (int)st);
        chml_document_destroy(d);
        return;
    }
    if (chml_document_write(d, "serialization_test.out") != CHML_OK) {
        failures++;
        printf("  FAIL %s: write failed\n", label);
        chml_document_destroy(d);
        return;
    }
    char *got = read_all("serialization_test.out", NULL);
    size_t need = strlen(PREFIX) + strlen(body) + 2;
    char *want = malloc(need);
    if (want) snprintf(want, need, "%s%s\n", PREFIX, body);
    if (!got || !want || strcmp(got, want) != 0) fail(label, got, want);
    else printf("  ok   %s\n", label);
    free(got); free(want);
    remove("serialization_test.out");
    chml_document_destroy(d);
}

static void expect_reject(const char *label, const ChmlNodeDef *def) {
    ChmlDocument *d = chml_document_create();
    ChmlStatus st = d ? chml_document_add(d, def) : CHML_ERR_MEMORY;
    if (st != CHML_ERR_ARGUMENT) {
        failures++;
        printf("  FAIL %s: expected CHML_ERR_ARGUMENT, got %d\n", label, (int)st);
    } else printf("  ok   %s\n", label);
    chml_document_destroy(d);
}

static int count_substring(const char *hay, const char *needle) {
    int n = 0;
    size_t len = strlen(needle);
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += len) n++;
    return n;
}

/* Builds <html><head>...</head><body><p>x</p></body></html> with head content. */
static void expect_document_with_head(const char *label, const ChmlNodeDef *head,
                                      const char *head_html) {
    ChmlNodeDef doc = CHML_ELEMENT("html", *head,
                                   CHML_ELEMENT("body", CHML_ELEMENT("p", CHML_TEXT("x"))));
    char want[512];
    snprintf(want, sizeof want, "<html>%s<body><p>x</p></body></html>", head_html);
    expect_body(label, &doc, want);
}

int main(void) {
    /* ---- ordinary text and attribute escaping ---- */
    {
        ChmlNodeDef p = CHML_ELEMENT("p", CHML_TEXT("a & b < c > d"));
        expect_body("ordinary text escapes & < >", &p,
                    "<p>a &amp; b &lt; c &gt; d</p>");

        /* A quote is significant only inside an attribute value. */
        const ChmlAttrDef q[] = {CHML_ATTR("title", "he said \"hi\" & <left>")};
        ChmlNodeDef a = CHML_LEAF_ATTRS("br", q, CHML_COUNT(q));
        expect_body("attribute values escape quotes and markup", &a,
                    "<br title=\"he said &quot;hi&quot; &amp; &lt;left&gt;\">");
    }

    /* ---- raw text: content must survive byte-for-byte ---- */
    {
        ChmlNodeDef s = CHML_ELEMENT("script", CHML_TEXT("if (a < b && c) go();"));
        expect_body("script content is not character-escaped", &s,
                    "<script>if (a < b && c) go();</script>");

        ChmlNodeDef st = CHML_ELEMENT("style", CHML_TEXT("a>b{color:red}"));
        expect_body("style content is not character-escaped", &st,
                    "<style>a>b{color:red}</style>");
    }

    /* ---- raw text: end-tag hazards ---- */
    {
        ChmlNodeDef h1 = CHML_ELEMENT("script", CHML_TEXT("var s = '</script>';"));
        expect_reject("script content containing </script> is rejected", &h1);

        ChmlNodeDef h2 = CHML_ELEMENT("script", CHML_TEXT("x</SCRIPT >y"));
        expect_reject("hazard match is case-insensitive and whitespace-terminated", &h2);

        ChmlNodeDef h3 = CHML_ELEMENT("style", CHML_TEXT("a{}</style/>"));
        expect_reject("slash-terminated </style/ hazard is rejected", &h3);

        ChmlNodeDef h4 = CHML_ELEMENT("script", CHML_TEXT("x</scriptx>y"));
        expect_body("</scriptx> is not an end tag and stays verbatim", &h4,
                    "<script>x</scriptx>y</script>");

        ChmlNodeDef h5 = CHML_ELEMENT("script", CHML_TEXT("x</scrip"));
        expect_body("partial tag name stays verbatim", &h5,
                    "<script>x</scrip</script>");

        /* "</script" at end of content is followed by the serializer's own "<",
           which is not a terminator, so the browser emits it as text. */
        ChmlNodeDef h6 = CHML_ELEMENT("script", CHML_TEXT("x</script"));
        expect_body("trailing </script without terminator stays verbatim", &h6,
                    "<script>x</script</script>");

        ChmlNodeDef h7 = CHML_ELEMENT("style", CHML_TEXT("</script>"));
        expect_body("</script> is harmless inside style", &h7,
                    "<style></script></style>");
    }

    /* ---- content model of text-only elements ---- */
    {
        ChmlNodeDef c1 = CHML_ELEMENT("script", CHML_LEAF("b"));
        expect_reject("element children inside script are rejected", &c1);

        ChmlNodeDef c2 = CHML_ELEMENT("textarea", CHML_LEAF("b"));
        expect_reject("element children inside textarea are rejected", &c2);

        ChmlNodeDef c3 = CHML_LEAF("plaintext");
        expect_reject("plaintext is rejected as unserializable", &c3);
    }

    /* ---- RCDATA (textarea, title): references decoded, markup not ---- */
    {
        ChmlNodeDef ta = CHML_ELEMENT("textarea", CHML_TEXT("a & b < c"));
        expect_body("textarea content is escaped like ordinary text", &ta,
                    "<textarea>a &amp; b &lt; c</textarea>");

        ChmlNodeDef ti = CHML_ELEMENT("title", CHML_TEXT("Q & A <draft>"));
        expect_body("title content is escaped like ordinary text", &ti,
                    "<title>Q &amp; A &lt;draft&gt;</title>");

        /* RCDATA escapes "<", so an embedded end tag cannot close the element. */
        ChmlNodeDef ta2 = CHML_ELEMENT("textarea", CHML_TEXT("</textarea>"));
        expect_body("textarea end tag in content is neutralised by escaping", &ta2,
                    "<textarea>&lt;/textarea&gt;</textarea>");
    }

    /* ---- boolean attributes ---- */
    {
        const ChmlAttrDef one[] = {CHML_BOOLEAN_ATTR("disabled")};
        ChmlNodeDef i1 = CHML_LEAF_ATTRS("input", one, CHML_COUNT(one));
        expect_body("boolean attribute serializes without a value", &i1,
                    "<input disabled>");

        const ChmlAttrDef mixed[] = {CHML_BOOLEAN_ATTR("checked"), CHML_ATTR("name", "x")};
        ChmlNodeDef i2 = CHML_LEAF_ATTRS("input", mixed, CHML_COUNT(mixed));
        expect_body("boolean and string attributes coexist", &i2,
                    "<input checked name=\"x\">");

        /* ARIA is string-valued: "false" must survive as a value. */
        const ChmlAttrDef aria[] = {CHML_ATTR("aria-disabled", "false")};
        ChmlNodeDef i3 = CHML_LEAF_ATTRS("input", aria, CHML_COUNT(aria));
        expect_body("aria-disabled stays string-valued", &i3,
                    "<input aria-disabled=\"false\">");

        const ChmlAttrDef bad_aria[] = {CHML_BOOLEAN_ATTR("aria-hidden")};
        ChmlNodeDef i4 = CHML_LEAF_ATTRS("div", bad_aria, CHML_COUNT(bad_aria));
        expect_reject("boolean kind is rejected on aria-* attributes", &i4);

        const ChmlAttrDef bad_val[] = {{.name = "disabled", .value = "false",
                                        .kind = CHML_ATTR_BOOLEAN}};
        ChmlNodeDef i5 = CHML_LEAF_ATTRS("input", bad_val, CHML_COUNT(bad_val));
        expect_reject("boolean attribute carrying a value is rejected", &i5);

        const ChmlAttrDef bad_kind[] = {{.name = "disabled", .value = NULL,
                                         .kind = (ChmlAttributeKind)99}};
        ChmlNodeDef i6 = CHML_LEAF_ATTRS("input", bad_kind, CHML_COUNT(bad_kind));
        expect_reject("unknown attribute kind is rejected", &i6);

        const ChmlAttrDef no_val[] = {{.name = "title", .value = NULL,
                                       .kind = CHML_ATTR_STRING}};
        ChmlNodeDef i7 = CHML_LEAF_ATTRS("input", no_val, CHML_COUNT(no_val));
        expect_reject("string attribute without a value is rejected", &i7);
    }

    /* ---- UTF-8 encoding declaration ---- */
    {
        ChmlNodeDef head_only = CHML_ELEMENT("head", CHML_ELEMENT("title", CHML_TEXT("T")));
        expect_document_with_head("charset is injected as the first head child",
                                  &head_only,
                                  "<head><meta charset=\"utf-8\"><title>T</title></head>");

        const ChmlAttrDef cs[] = {CHML_ATTR("charset", "UTF-8")};
        ChmlNodeDef meta = CHML_LEAF_ATTRS("meta", cs, CHML_COUNT(cs));
        ChmlNodeDef head_existing = CHML_ELEMENT("head", meta);
        expect_document_with_head("an existing UTF-8 declaration is not duplicated",
                                  &head_existing,
                                  "<head><meta charset=\"UTF-8\"></head>");

        const ChmlAttrDef cs_latin[] = {CHML_ATTR("charset", "iso-8859-1")};
        ChmlNodeDef meta_latin = CHML_LEAF_ATTRS("meta", cs_latin, CHML_COUNT(cs_latin));
        ChmlNodeDef head_latin = CHML_ELEMENT("head", meta_latin);

        ChmlDocument *d = chml_document_create();
        ChmlNodeDef doc_latin = CHML_ELEMENT("html", head_latin);
        if (chml_document_add(d, &doc_latin) != CHML_OK) {
            failures++; puts("  FAIL conflicting charset document did not build");
        } else {
            ChmlStatus st = chml_document_write(d, "serialization_test.out");
            if (st != CHML_ERR_ARGUMENT) {
                failures++;
                printf("  FAIL conflicting charset writes %d, expected CHML_ERR_ARGUMENT\n", (int)st);
            } else printf("  ok   conflicting charset declaration is rejected\n");
            remove("serialization_test.out");
        }
        chml_document_destroy(d);

        /* http-equiv form, conflicting. */
        const ChmlAttrDef he[] = {CHML_ATTR("http-equiv", "Content-Type"),
                                  CHML_ATTR("content", "text/html; charset=iso-8859-1")};
        ChmlNodeDef meta_he = CHML_LEAF_ATTRS("meta", he, CHML_COUNT(he));
        ChmlNodeDef head_he = CHML_ELEMENT("head", meta_he);
        ChmlDocument *d2 = chml_document_create();
        ChmlNodeDef doc_he = CHML_ELEMENT("html", head_he);
        if (chml_document_add(d2, &doc_he) != CHML_OK) {
            failures++; puts("  FAIL http-equiv document did not build");
        } else {
            if (chml_document_write(d2, "serialization_test.out") != CHML_ERR_ARGUMENT) {
                failures++; puts("  FAIL conflicting http-equiv charset is not rejected");
            } else puts("  ok   conflicting http-equiv charset is rejected");
            remove("serialization_test.out");
        }
        chml_document_destroy(d2);

        /* http-equiv form, already UTF-8: no injection, no duplicate. */
        const ChmlAttrDef he8[] = {CHML_ATTR("http-equiv", "content-type"),
                                   CHML_ATTR("content", "text/html; charset=utf-8")};
        ChmlNodeDef meta_he8 = CHML_LEAF_ATTRS("meta", he8, CHML_COUNT(he8));
        ChmlNodeDef head_he8 = CHML_ELEMENT("head", meta_he8);
        expect_document_with_head("a utf-8 http-equiv declaration suppresses injection",
                                  &head_he8,
                                  "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\"></head>");

        /* No <head>: the document is treated as a fragment. */
        ChmlNodeDef p = CHML_ELEMENT("p", CHML_TEXT("fragment"));
        expect_body("fragment without head gets no charset declaration", &p,
                    "<p>fragment</p>");
    }

    /* ---- UTF-8 byte output ---- */
    {
        ChmlNodeDef head = CHML_ELEMENT("head", CHML_ELEMENT("title", CHML_TEXT("T")));
        ChmlNodeDef doc = CHML_ELEMENT("html", head,
                                       CHML_ELEMENT("body", CHML_ELEMENT("p", CHML_TEXT("caf\xC3\xA9 \xE2\x80\x94 ok"))));
        ChmlDocument *d = chml_document_create();
        if (chml_document_add(d, &doc) != CHML_OK) {
            failures++; puts("  FAIL utf-8 document did not build");
        } else if (chml_document_write(d, "serialization_test.out") != CHML_OK) {
            failures++; puts("  FAIL utf-8 document did not write");
        } else {
            size_t n = 0;
            char *got = read_all("serialization_test.out", &n);
            /* C3 A9 = U+00E9, E2 80 94 = U+2014 */
            if (!got || !memchr(got, 0xC3, n)) {
                failures++; puts("  FAIL utf-8 bytes missing from output");
            } else if (count_substring(got, "charset=\"utf-8\"") != 1) {
                failures++; puts("  FAIL expected exactly one utf-8 declaration");
            } else if (strstr(got, "caf\xC3\xA9 \xE2\x80\x94 ok") == NULL) {
                failures++; puts("  FAIL non-ASCII text was modified");
            } else puts("  ok   non-ASCII text is written as UTF-8 with one declaration");
            free(got);
            remove("serialization_test.out");
        }
        chml_document_destroy(d);
    }

    /* ---- failed materialization must not disturb the document ---- */
    {
        ChmlDocument *d = chml_document_create();
        ChmlNodeDef keep = CHML_ELEMENT("p", CHML_TEXT("kept"));
        ChmlNodeDef bad = CHML_ELEMENT("script", CHML_TEXT("</script>"));
        if (chml_document_add(d, &keep) != CHML_OK) { failures++; puts("  FAIL keep root"); }
        if (chml_document_add(d, &bad) != CHML_ERR_ARGUMENT) {
            failures++; puts("  FAIL hazardous script was not rejected");
        }
        if (chml_document_write(d, "serialization_test.out") != CHML_OK) {
            failures++; puts("  FAIL write after rejected add");
        } else {
            char *got = read_all("serialization_test.out", NULL);
            if (!got || strcmp(got, PREFIX "<p>kept</p>\n") != 0) {
                fail("rejected add leaves no partial state", got, PREFIX "<p>kept</p>\n");
            } else puts("  ok   rejected add leaves no partial state");
            free(got);
            remove("serialization_test.out");
        }
        chml_document_destroy(d);
    }

    if (failures) { printf("%d serialization fixture(s) failed\n", failures); return 1; }
    puts("serialization fixtures passed");
    return 0;
}
