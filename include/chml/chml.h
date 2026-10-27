#ifndef CHML_H
#define CHML_H
#include <stddef.h>

typedef struct ChmlDocument ChmlDocument;
typedef struct ChmlNode ChmlNode;
typedef enum { CHML_OK=0, CHML_ERR_ARGUMENT, CHML_ERR_MEMORY, CHML_ERR_IO, CHML_ERR_POLICY } ChmlStatus;

/* How an attribute value is interpreted at serialization time.
   CHML_ATTR_STRING  -> name="value" (ARIA, data-*, and every other attribute)
   CHML_ATTR_BOOLEAN -> name only, no value (HTML boolean attributes such as
                        disabled, checked, required, hidden)

   The two are never inferred from each other: a string value of "false" stays
   a string, so aria-disabled="false" is distinct from a boolean disabled. */
typedef enum { CHML_ATTR_STRING = 0, CHML_ATTR_BOOLEAN = 1 } ChmlAttributeKind;

typedef struct {
    const char *name;
    const char *value;      /* must be NULL for CHML_ATTR_BOOLEAN */
    ChmlAttributeKind kind; /* zero value keeps positional initialisation valid */
} ChmlAttrDef;

typedef struct ChmlNodeDef {
    const char *tag;
    const char *text;
    const ChmlAttrDef *attributes;
    size_t attribute_count;
    const struct ChmlNodeDef *children;
    size_t child_count;
} ChmlNodeDef;

#define CHML_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define CHML_ATTR(n,v) ((ChmlAttrDef){.name=(n),.value=(v),.kind=CHML_ATTR_STRING})
#define CHML_BOOLEAN_ATTR(n) ((ChmlAttrDef){.name=(n),.value=NULL,.kind=CHML_ATTR_BOOLEAN})
#define CHML_ATTRS(...) .attributes=(const ChmlAttrDef[]){__VA_ARGS__}, .attribute_count=CHML_COUNT(((const ChmlAttrDef[]){__VA_ARGS__}))

/* The CHML_* builders below create compound literals with automatic storage.
   They are valid only inside the function body that uses them: a ChmlNodeDef
   that contains children must never be returned by value or stored past the
   end of the enclosing block. See docs/decisions/ADR-0004. */
#define CHML_ELEMENT(t,...) ((ChmlNodeDef){.tag=(t),.children=(const ChmlNodeDef[]){__VA_ARGS__},.child_count=CHML_COUNT(((const ChmlNodeDef[]){__VA_ARGS__}))})
#define CHML_TEXT(t) ((ChmlNodeDef){.text=(t)})
#define CHML_LEAF(t) ((ChmlNodeDef){.tag=(t)})
#define CHML_LEAF_ATTRS(t,attrs,count) ((ChmlNodeDef){.tag=(t),.attributes=(attrs),.attribute_count=(count)})
#define CHML_WITH_ATTRS(t,attrs,count,...) ((ChmlNodeDef){.tag=(t),.attributes=(attrs),.attribute_count=(count),.children=(const ChmlNodeDef[]){__VA_ARGS__},.child_count=CHML_COUNT(((const ChmlNodeDef[]){__VA_ARGS__}))})

ChmlDocument *chml_document_create(void);
ChmlStatus chml_document_add(ChmlDocument *, const ChmlNodeDef *);
ChmlStatus chml_document_write(const ChmlDocument *, const char *path);
void chml_document_destroy(ChmlDocument *);

#endif
