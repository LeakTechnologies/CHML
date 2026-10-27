#ifndef CHML_STYLE_H
#define CHML_STYLE_H
#include <stddef.h>
#include "chml.h"
typedef struct { const char *property; const char *value; } ChmlDeclaration;
typedef struct {
    const char *selector;
    const char *condition; /* NULL or CSS media query condition, without @media */
    const ChmlDeclaration *declarations;
    size_t declaration_count;
} ChmlRuleDef;
typedef struct { const ChmlRuleDef *rules; size_t rule_count; } ChmlSheetDef;
#define CHML_DECL(p,v) ((ChmlDeclaration){(p),(v)})
#define CHML_RULE(s,d) ((ChmlRuleDef){(s),NULL,(d),sizeof(d)/sizeof((d)[0])})
#define CHML_MEDIA_RULE(q,s,d) ((ChmlRuleDef){(s),(q),(d),sizeof(d)/sizeof((d)[0])})
ChmlStatus chml_stylesheet_write(const ChmlSheetDef *sheet, const char *path);
#endif
