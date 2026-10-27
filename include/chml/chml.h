#ifndef CHML_H
#define CHML_H
#include <stddef.h>

typedef struct ChmlDocument ChmlDocument;
typedef struct ChmlNode ChmlNode;
typedef enum { CHML_OK=0, CHML_ERR_ARGUMENT, CHML_ERR_MEMORY, CHML_ERR_IO } ChmlStatus;
typedef struct { const char *name; const char *value; } ChmlAttrDef;
typedef struct ChmlNodeDef {
    const char *tag;
    const char *text;
    const ChmlAttrDef *attributes;
    size_t attribute_count;
    const struct ChmlNodeDef *children;
    size_t child_count;
} ChmlNodeDef;
#define CHML_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define CHML_ATTR(n,v) ((ChmlAttrDef){.name=(n),.value=(v)})
#define CHML_ATTRS(...) .attributes=(const ChmlAttrDef[]){__VA_ARGS__}, .attribute_count=CHML_COUNT(((const ChmlAttrDef[]){__VA_ARGS__}))
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
