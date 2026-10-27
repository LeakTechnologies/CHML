#include "chml/style.h"
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
ChmlStatus chml_stylesheet_write(const ChmlSheetDef *sheet,const char *path) {
    if (!sheet||!path||(sheet->rule_count&&!sheet->rules)) return CHML_ERR_ARGUMENT;
    /* Validate before opening destination; this is intentionally a restricted M0.3 subset. */
    for(size_t i=0;i<sheet->rule_count;i++) {
        const ChmlRuleDef *r=&sheet->rules[i];
        if(!safe(r->selector)||(r->condition&&!safe(r->condition))||
           (r->declaration_count&&!r->declarations))return CHML_ERR_ARGUMENT;
        for(size_t j=0;j<r->declaration_count;j++)
            if(!property_name(r->declarations[j].property)||!safe(r->declarations[j].value))return CHML_ERR_ARGUMENT;
    }
    FILE *f=fopen(path,"wb");if(!f)return CHML_ERR_IO;
    int ok=1;
    for(size_t i=0;i<sheet->rule_count&&ok;i++) {
        const ChmlRuleDef *r=&sheet->rules[i];
        if(r->condition&&fprintf(f,"@media %s{",r->condition)<0)ok=0;
        if(ok&&fprintf(f,"%s{",r->selector)<0)ok=0;
        for(size_t j=0;j<r->declaration_count&&ok;j++)
            if(fprintf(f,"%s:%s;",r->declarations[j].property,r->declarations[j].value)<0)ok=0;
        if(ok&&fputs(r->condition?"}}\n":"}\n",f)==EOF)ok=0;
    }
    if(fclose(f)!=0)ok=0;
    return ok?CHML_OK:CHML_ERR_IO;
}
