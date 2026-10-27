/* URL policy for designated URL-bearing attributes (M0-S2b).
   Restricted serialization model: HTML and SVG are modelled, MathML fails closed. */
#ifndef CHML_POLICY_H
#define CHML_POLICY_H
#include "chml/chml.h"

typedef enum { CHML_NS_HTML = 0, CHML_NS_SVG = 1, CHML_NS_MATHML = 2 } ChmlNs;

/* Namespace of the element itself, given its parent's child-namespace. */
ChmlNs chml_ns_of_element(ChmlNs parent_ns, const char *tag);
/* Namespace applied to that element's children. */
ChmlNs chml_ns_for_children(ChmlNs elem_ns, const char *tag, const char *encoding);

typedef enum {
    CHML_ATTR_IGNORE = 0,   /* not governed by this policy */
    CHML_ATTR_URL = 1,      /* URL-bearing: classify the value */
    CHML_ATTR_REFUSED = 2   /* refused outright, any value, including empty */
} ChmlAttrKind;

ChmlAttrKind chml_attr_kind(ChmlNs ns, const char *element, const char *attribute);

/* Classifies a logical (unescaped) attribute value.
   Returns CHML_OK, CHML_ERR_POLICY, or CHML_ERR_MEMORY. */
ChmlStatus chml_url_policy(ChmlNs ns, const char *element, const char *attribute,
                           const char *value, ChmlAttrKind kind);

#endif
