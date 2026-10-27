#include "chml/style.h"
#include "chml_io.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
static int safe(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p=(const unsigned char *)s;*p;p++)
        if (*p=='{' || *p=='}' || *p==';' || *p=='\r' || *p=='\n' || *p<32) return 0;
    return 1;
}
static int property_name(const char *s) {
    if (!safe(s)) return 0;
    for (const unsigned char *p=(const unsigned char *)s;*p;p++)
        if (!(isalnum(*p)||*p=='-')) return 0;
    return 1;
}
typedef struct { const ChmlSheetDef *sheet; } SheetOutput;
static int write_sheet(FILE *f,const void *ctx) {
    const ChmlSheetDef *sheet=((const SheetOutput *)ctx)->sheet;
    for(size_t i=0;i<sheet->rule_count;i++) {
        const ChmlRuleDef *r=&sheet->rules[i];
        if(r->condition&&fprintf(f,"@media %s{",r->condition)<0)return 0;
        if(fprintf(f,"%s{",r->selector)<0)return 0;
        for(size_t j=0;j<r->declaration_count;j++)
            if(fprintf(f,"%s:%s;",r->declarations[j].property,r->declarations[j].value)<0)return 0;
        if(fputs(r->condition?"}}\n":"}\n",f)==EOF)return 0;
    }
    return 1;
}
ChmlStatus chml_stylesheet_write(const ChmlSheetDef *sheet,const char *path) {
    if (!sheet||!path||(sheet->rule_count&&!sheet->rules)) return CHML_ERR_ARGUMENT;
    /* Validate before touching the filesystem; this is intentionally a restricted M0.3 subset. */
    for(size_t i=0;i<sheet->rule_count;i++) {
        const ChmlRuleDef *r=&sheet->rules[i];
        if(!safe(r->selector)||(r->condition&&!safe(r->condition))||
           (r->declaration_count&&!r->declarations))return CHML_ERR_ARGUMENT;
        for(size_t j=0;j<r->declaration_count;j++)
            if(!property_name(r->declarations[j].property)||!safe(r->declarations[j].value))return CHML_ERR_ARGUMENT;
    }
    SheetOutput out={sheet};
    return chml_write_file_atomic(path,write_sheet,&out);
}
