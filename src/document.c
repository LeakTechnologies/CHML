#include "chml/chml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

struct ChmlNode {
    char *tag, *text;
    ChmlAttrDef *attrs;
    size_t attr_count;
    struct ChmlNode *children;
    size_t child_count;
};
struct ChmlDocument { struct ChmlNode *roots; size_t root_count; };
static char *copy(const char *s) {
    if (!s) return NULL;
    size_t n=strlen(s)+1;
    char *p=malloc(n);
    if(p) memcpy(p,s,n);
    return p;
}
static void free_node(struct ChmlNode *n) {
    free(n->tag); free(n->text);
    for(size_t i=0;i<n->attr_count;i++) { free((void*)n->attrs[i].name);free((void*)n->attrs[i].value); }
    free(n->attrs);
    for(size_t i=0;i<n->child_count;i++) free_node(&n->children[i]);
    free(n->children);
    memset(n,0,sizeof(*n));
}
static int valid_name(const char *s) {
    if(!s || !*s || !isalpha((unsigned char)*s)) return 0;
    for(;*s;s++) if(!isalnum((unsigned char)*s) && *s!='-' && *s!='_' && *s!=':') return 0;
    return 1;
}
/* HTML element and attribute names are ASCII case-insensitive. Compare with an
   explicit ASCII fold so behaviour does not depend on the process locale. */
static int ascii_ci_equal(const char *a,const char *b) {
    for(;;a++,b++) {
        unsigned char ca=(unsigned char)*a,cb=(unsigned char)*b;
        if(ca>='A'&&ca<='Z')ca=(unsigned char)(ca-'A'+'a');
        if(cb>='A'&&cb<='Z')cb=(unsigned char)(cb-'A'+'a');
        if(ca!=cb)return 0;
        if(!ca)return 1;
    }
}
static int is_void(const char *tag);
static ChmlStatus clone(struct ChmlNode *out,const ChmlNodeDef *src,unsigned depth) {
    if(!src || depth>128 || (src->tag && !valid_name(src->tag)) || (!src->tag && (src->child_count || src->attribute_count)) || (src->child_count && !src->children) || (src->attribute_count && !src->attributes)) return CHML_ERR_ARGUMENT;
    if(src->tag && is_void(src->tag) && (src->text || src->child_count)) return CHML_ERR_ARGUMENT;
    if(src->tag) {out->tag=copy(src->tag);if(!out->tag)return CHML_ERR_MEMORY;}
    if(src->text) {out->text=copy(src->text);if(!out->text)return CHML_ERR_MEMORY;}
    if(src->attribute_count) {
        out->attrs=calloc(src->attribute_count,sizeof(*out->attrs));if(!out->attrs)return CHML_ERR_MEMORY;
        out->attr_count=src->attribute_count;
        for(size_t i=0;i<out->attr_count;i++) {
            if(!valid_name(src->attributes[i].name) || !src->attributes[i].value)return CHML_ERR_ARGUMENT;
            /* A duplicate attribute would be silently dropped by the HTML parser,
               which loses author data. Reject it instead of emitting it. */
            for(size_t j=0;j<i;j++)
                if(ascii_ci_equal(out->attrs[j].name,src->attributes[i].name))return CHML_ERR_ARGUMENT;
            out->attrs[i].name=copy(src->attributes[i].name);
            out->attrs[i].value=copy(src->attributes[i].value);
            if(!out->attrs[i].name || !out->attrs[i].value)return CHML_ERR_MEMORY;
        }
    }
    if(src->child_count) {
        out->children=calloc(src->child_count,sizeof(*out->children));if(!out->children)return CHML_ERR_MEMORY;
        out->child_count=src->child_count;
        for(size_t i=0;i<out->child_count;i++) {
            ChmlStatus st=clone(&out->children[i],&src->children[i],depth+1);
            if(st!=CHML_OK)return st;
        }
    }
    return CHML_OK;
}
ChmlDocument *chml_document_create(void) {return calloc(1,sizeof(ChmlDocument));}
ChmlStatus chml_document_add(ChmlDocument *doc,const ChmlNodeDef *def) {
    if(!doc||!def)return CHML_ERR_ARGUMENT;
    struct ChmlNode tmp={0};
    ChmlStatus st=clone(&tmp,def,0);
    if(st!=CHML_OK){free_node(&tmp);return st;}
    if (doc->root_count == SIZE_MAX / sizeof(*doc->roots)) {free_node(&tmp);return CHML_ERR_MEMORY;}
    struct ChmlNode *next=realloc(doc->roots,(doc->root_count+1)*sizeof(*next));
    if(!next){free_node(&tmp);return CHML_ERR_MEMORY;}
    doc->roots=next;doc->roots[doc->root_count++]=tmp;
    return CHML_OK;
}
static int escaped(FILE *f,const char *s,int attr) {
    for(;*s;s++) {
        const char *r=NULL;
        switch(*s){case '&':r="&amp;";break;case '<':r="&lt;";break;case '>':r="&gt;";break;case '"':if(attr)r="&quot;";break;}
        if(r){if(fputs(r,f)==EOF)return 0;}else if(fputc((unsigned char)*s,f)==EOF)return 0;
    }
    return 1;
}
static int is_void(const char *tag) {
    const char *names[]={"area","base","br","col","embed","hr","img","input","link","meta","param","source","track","wbr"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);i++)if(ascii_ci_equal(tag,names[i]))return 1;
    return 0;
}
static int emit(FILE *f,const struct ChmlNode *n) {
    if(!n->tag)return n->text ? escaped(f,n->text,0):1;
    if(fprintf(f,"<%s",n->tag)<0)return 0;
    for(size_t i=0;i<n->attr_count;i++) {
        if(fprintf(f," %s=\"",n->attrs[i].name)<0 || !escaped(f,n->attrs[i].value,1) || fputc('"',f)==EOF)return 0;
    }
    if(fputc('>',f)==EOF)return 0;
    if(is_void(n->tag))return n->child_count==0 && n->text==NULL;
    if(n->text && !escaped(f,n->text,0))return 0;
    for(size_t i=0;i<n->child_count;i++)if(!emit(f,&n->children[i]))return 0;
    return fprintf(f,"</%s>",n->tag)>=0;
}
ChmlStatus chml_document_write(const ChmlDocument *doc,const char *path) {
    if(!doc||!path)return CHML_ERR_ARGUMENT;
    FILE *f=fopen(path,"wb");if(!f)return CHML_ERR_IO;
    int ok=fputs("<!doctype html>\n",f)!=EOF;
    for(size_t i=0;ok&&i<doc->root_count;i++)ok=emit(f,&doc->roots[i]);
    if(ok)ok=fputc('\n',f)!=EOF;
    if(fclose(f)!=0)ok=0;
    return ok?CHML_OK:CHML_ERR_IO;
}
void chml_document_destroy(ChmlDocument *doc) {
    if(!doc)return;
    for(size_t i=0;i<doc->root_count;i++)free_node(&doc->roots[i]);
    free(doc->roots);free(doc);
}
