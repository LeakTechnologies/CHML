#include "chml_policy.h"
#include <string.h>
#include <stdint.h>

static int eq_ci(const char *a, const char *b) {
    for (;; a++, b++) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}
static int eq_cs(const char *a, const char *b) { return strcmp(a, b) == 0; }

static int name_in_ci(const char *tag, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++) if (eq_ci(tag, list[i])) return 1;
    return 0;
}

ChmlNs chml_ns_of_element(ChmlNs parent_ns, const char *tag) {
    if (!tag) return parent_ns;
    if (parent_ns == CHML_NS_HTML) {
        if (eq_ci(tag, "svg")) return CHML_NS_SVG;
        if (eq_ci(tag, "math")) return CHML_NS_MATHML;
    }
    return parent_ns;
}

ChmlNs chml_ns_for_children(ChmlNs elem_ns, const char *tag, const char *encoding) {
    if (!tag) return elem_ns;
    if (elem_ns == CHML_NS_SVG && eq_ci(tag, "foreignObject")) return CHML_NS_HTML;
    if (elem_ns == CHML_NS_MATHML && eq_ci(tag, "annotation-xml") && encoding &&
        (eq_ci(encoding, "text/html") || eq_ci(encoding, "application/xhtml+xml")))
        return CHML_NS_HTML;
    return elem_ns;
}

ChmlAttrKind chml_attr_kind(ChmlNs ns, const char *element, const char *attribute) {
    if (!element || !attribute) return CHML_ATTR_IGNORE;

    /* Refused outright, before any value rule. */
    if (ns == CHML_NS_HTML) {
        if (eq_ci(attribute, "srcset") || eq_ci(attribute, "srcdoc")) return CHML_ATTR_REFUSED;
        if (eq_ci(element, "object") && eq_ci(attribute, "data")) return CHML_ATTR_REFUSED;
    }

    if (ns == CHML_NS_MATHML) {
        /* MathML is out of scope: fail closed on URL-bearing attributes. */
        if (eq_ci(attribute, "href") || eq_ci(attribute, "src") || eq_ci(attribute, "xlink:href"))
            return CHML_ATTR_REFUSED;
        return CHML_ATTR_IGNORE;
    }

    if (ns == CHML_NS_SVG) {
        static const char *const elems[] = { "a", "use", "image" };
        if (!name_in_ci(element, elems, sizeof elems / sizeof *elems)) return CHML_ATTR_IGNORE;
        if (eq_cs(attribute, "xlink:href") || eq_ci(attribute, "href")) return CHML_ATTR_URL;
        return CHML_ATTR_IGNORE;
    }

    if (eq_ci(attribute, "href")) {
        static const char *const nav[] = { "a", "area", "link" };
        return name_in_ci(element, nav, sizeof nav / sizeof *nav) ? CHML_ATTR_URL : CHML_ATTR_IGNORE;
    }
    if (eq_ci(attribute, "src")) {
        static const char *const res[] = { "img", "script", "iframe", "source", "track", "embed" };
        return name_in_ci(element, res, sizeof res / sizeof *res) ? CHML_ATTR_URL : CHML_ATTR_IGNORE;
    }
    if (eq_ci(attribute, "action") && eq_ci(element, "form")) return CHML_ATTR_URL;
    if (eq_ci(attribute, "formaction") && (eq_ci(element, "button") || eq_ci(element, "input")))
        return CHML_ATTR_URL;
    if (eq_ci(attribute, "poster") && eq_ci(element, "video")) return CHML_ATTR_URL;
    return CHML_ATTR_IGNORE;
}

/* Empty value accepted only where HTML gives it meaning. */
static int empty_allowed(ChmlNs ns, const char *element, const char *attribute) {
    if (ns != CHML_NS_HTML) return 0;
    if (eq_ci(attribute, "href")) return eq_ci(element, "a") || eq_ci(element, "area");
    if (eq_ci(attribute, "action")) return eq_ci(element, "form");
    return 0;
}

static int scheme_allowed(const char *scheme, size_t len, ChmlNs ns,
                          const char *element, const char *attribute) {
    char buf[16];
    if (len == 0 || len >= sizeof buf) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = scheme[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[i] = c;
    }
    buf[len] = '\0';

    int navigation = eq_ci(attribute, "href") && ns == CHML_NS_HTML &&
                     (eq_ci(element, "a") || eq_ci(element, "area") || eq_ci(element, "link"));
    if (strcmp(buf, "https") == 0 || strcmp(buf, "http") == 0) return 1;
    if (navigation && (strcmp(buf, "mailto") == 0 || strcmp(buf, "tel") == 0)) return 1;
    return 0;
}

ChmlStatus chml_url_policy(ChmlNs ns, const char *element, const char *attribute,
                           const char *value, ChmlAttrKind kind) {
    if (kind == CHML_ATTR_REFUSED) return CHML_ERR_POLICY;
    if (kind != CHML_ATTR_URL) return CHML_OK;
    if (!value) return CHML_ERR_POLICY;

    size_t n = strlen(value);

    /* R1: strip trailing space/tab only. */
    while (n > 0 && (value[n - 1] == ' ' || value[n - 1] == '\t')) n--;

    /* R2: empty, evaluated after context. */
    if (n == 0) return empty_allowed(ns, element, attribute) ? CHML_OK : CHML_ERR_POLICY;

    /* R3: refuse controls, spaces and backslashes anywhere. */
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c <= 0x1F || c == 0x7F || c == ' ' || c == '\\') return CHML_ERR_POLICY;
    }

    /* R4: protocol-relative. */
    if (n >= 2 && value[0] == '/' && value[1] == '/') return CHML_ERR_POLICY;

    /* R5: explicit relative forms. */
    if (value[0] == '/') return CHML_OK;
    if (n >= 2 && value[0] == '.' && value[1] == '/') return CHML_OK;
    if (n >= 3 && value[0] == '.' && value[1] == '.' && value[2] == '/') return CHML_OK;

    /* R6: fragment or query. */
    if (value[0] == '#' || value[0] == '?') return CHML_OK;

    /* R7: first segment ends at '/', '?' or '#'. */
    size_t seg = 0;
    while (seg < n && value[seg] != '/' && value[seg] != '?' && value[seg] != '#') seg++;

    /* R8: a colon inside the first segment introduces a scheme candidate. */
    size_t colon = SIZE_MAX;
    for (size_t i = 0; i < seg; i++) if (value[i] == ':') { colon = i; break; }
    if (colon != SIZE_MAX) {
        if (colon == 0) return CHML_ERR_POLICY;
        for (size_t i = 0; i < colon; i++) {
            unsigned char c = (unsigned char)value[i];
            int alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            int rest = alpha || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
            if (i == 0 ? !alpha : !rest) return CHML_ERR_POLICY;
        }
        return scheme_allowed(value, colon, ns, element, attribute) ? CHML_OK : CHML_ERR_POLICY;
    }

    /* R9: ordinary path-relative. */
    return CHML_OK;
}
