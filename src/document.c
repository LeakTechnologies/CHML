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
static unsigned char ascii_fold(unsigned char c) {
    return (c>='A'&&c<='Z')?(unsigned char)(c-'A'+'a'):c;
}
static int ascii_ci_equal(const char *a,const char *b) {
    for(;;a++,b++) {
        unsigned char ca=ascii_fold((unsigned char)*a),cb=ascii_fold((unsigned char)*b);
        if(ca!=cb)return 0;
        if(!ca)return 1;
    }
}
static int ascii_ci_prefix(const char *s,const char *prefix) {
    for(size_t i=0;prefix[i];i++)
        if(ascii_fold((unsigned char)s[i])!=ascii_fold((unsigned char)prefix[i]))return 0;
    return 1;
}

/* ---- Text processing contexts (HTML Living Standard, section 13.2) ----
   ORDINARY: character references are decoded, so & < > must be escaped.
   RCDATA (title, textarea): character references are decoded, elements are not,
            so escaping exactly as ordinary text is correct.
   RAW (script, style, xmp, iframe, noembed, noframes): character references are
            NOT decoded, so text is emitted verbatim. A sequence that closes the
            element early is rejected at materialization instead. */
typedef enum { CHML_TEXT_ORDINARY=0, CHML_TEXT_RCDATA=1, CHML_TEXT_RAW=2 } ChmlTextKind;

static int name_in(const char *tag,const char *const *list,size_t count) {
    for(size_t i=0;i<count;i++) if(ascii_ci_equal(tag,list[i])) return 1;
    return 0;
}
static ChmlTextKind text_kind(const char *tag) {
    static const char *const raw[]={"script","style","xmp","iframe","noembed","noframes"};
    static const char *const rcdata[]={"title","textarea"};
    if(name_in(tag,raw,CHML_COUNT(raw))) return CHML_TEXT_RAW;
    if(name_in(tag,rcdata,CHML_COUNT(rcdata))) return CHML_TEXT_RCDATA;
    return CHML_TEXT_ORDINARY;
}
static int is_void(const char *tag) {
    static const char *const names[]={"area","base","br","col","embed","hr","img","input","link","meta","param","source","track","wbr"};
    return name_in(tag,names,CHML_COUNT(names));
}
/* <plaintext> consumes every following byte including its own end tag, so it
   cannot be serialized into a well-formed document. Rejected, not emulated. */
static int is_plaintext(const char *tag) { return ascii_ci_equal(tag,"plaintext"); }

static int is_ascii_ws(unsigned char c) { return c=='\t'||c=='\n'||c=='\f'||c==' '; }

/* Raw-text content ends the element when "</" is followed by the element's own
   name and then one of TAB, LF, FF, SPACE, "/" or ">" (tokenizer "end tag name"
   state). A trailing "</script" with nothing after it is NOT a hazard: the
   serializer's own closing tag supplies "<", which is not a terminator, so the
   browser emits the text and then closes the element. */
static int has_end_tag_hazard(const char *text,const char *tag) {
    size_t tlen=strlen(tag);
    for(const char *p=strchr(text,'<');p;p=strchr(p+1,'<')) {
        if(p[1]!='/')continue;
        const char *q=p+2;
        size_t i=0;
        while(i<tlen && ascii_fold((unsigned char)q[i])==ascii_fold((unsigned char)tag[i]))i++;
        if(i!=tlen)continue;
        unsigned char term=(unsigned char)q[tlen];
        if(term=='\0')continue;
        if(term=='/'||term=='>'||is_ascii_ws(term))return 1;
    }
    return 0;
}

static int is_aria(const char *name) { return ascii_ci_prefix(name,"aria-"); }

/* Collects the content of a text-only element (its own .text plus every text
   child, in serialization order) so the end-tag hazard check sees the whole
   run. *out is NULL for an empty element. Fails closed: any allocation failure
   is reported so the caller cannot skip validation. */
static ChmlStatus text_content(const ChmlNodeDef *src,char **out) {
    *out=NULL;
    size_t total=src->text?strlen(src->text):0;
    for(size_t i=0;i<src->child_count;i++) {
        if(!src->children[i].text)continue;
        size_t n=strlen(src->children[i].text);
        if(total>SIZE_MAX-1-n)return CHML_ERR_MEMORY;
        total+=n;
    }
    if(total==0)return CHML_OK;
    char *buf=malloc(total+1);
    if(!buf)return CHML_ERR_MEMORY;
    size_t off=0;
    if(src->text){size_t n=strlen(src->text);memcpy(buf+off,src->text,n);off+=n;}
    for(size_t i=0;i<src->child_count;i++) {
        if(!src->children[i].text)continue;
        size_t n=strlen(src->children[i].text);
        memcpy(buf+off,src->children[i].text,n);off+=n;
    }
    buf[off]=0;
    *out=buf;
    return CHML_OK;
}

static ChmlStatus clone(struct ChmlNode *out,const ChmlNodeDef *src,unsigned depth) {
    if(!src || depth>128 || (src->tag && !valid_name(src->tag)) || (!src->tag && (src->child_count || src->attribute_count)) || (src->child_count && !src->children) || (src->attribute_count && !src->attributes)) return CHML_ERR_ARGUMENT;
    if(src->tag && is_void(src->tag) && (src->text || src->child_count)) return CHML_ERR_ARGUMENT;
    if(src->tag && is_plaintext(src->tag)) return CHML_ERR_ARGUMENT;
    if(src->tag) {
        ChmlTextKind kind=text_kind(src->tag);
        if(kind!=CHML_TEXT_ORDINARY) {
            /* Raw-text and RCDATA elements hold text, never element children. */
            for(size_t i=0;i<src->child_count;i++)
                if(src->children[i].tag)return CHML_ERR_ARGUMENT;
        }
        if(kind==CHML_TEXT_RAW) {
            char *content=NULL;
            ChmlStatus st=text_content(src,&content);
            if(st!=CHML_OK)return st;
            if(content) {
                int hazard=has_end_tag_hazard(content,src->tag);
                free(content);
                if(hazard)return CHML_ERR_ARGUMENT;
            }
        }
    }
    if(src->tag) {out->tag=copy(src->tag);if(!out->tag)return CHML_ERR_MEMORY;}
    if(src->text) {out->text=copy(src->text);if(!out->text)return CHML_ERR_MEMORY;}
    if(src->attribute_count) {
        out->attrs=calloc(src->attribute_count,sizeof(*out->attrs));if(!out->attrs)return CHML_ERR_MEMORY;
        out->attr_count=src->attribute_count;
        for(size_t i=0;i<out->attr_count;i++) {
            const ChmlAttrDef *a=&src->attributes[i];
            if(!valid_name(a->name)) return CHML_ERR_ARGUMENT;
            switch(a->kind) {
                case CHML_ATTR_STRING:
                    if(!a->value) return CHML_ERR_ARGUMENT;
                    break;
                case CHML_ATTR_BOOLEAN:
                    /* A boolean attribute has no value. Never reinterpret a
                       string such as "false"; that distinction belongs to the
                       author. ARIA attributes are never HTML booleans. */
                    if(a->value) return CHML_ERR_ARGUMENT;
                    if(is_aria(a->name)) return CHML_ERR_ARGUMENT;
                    break;
                default:
                    return CHML_ERR_ARGUMENT;
            }
            /* A duplicate attribute would be silently dropped by the HTML parser,
               which loses author data. Reject it instead of emitting it. */
            for(size_t j=0;j<i;j++)
                if(ascii_ci_equal(out->attrs[j].name,a->name))return CHML_ERR_ARGUMENT;
            out->attrs[i].name=copy(a->name);
            out->attrs[i].value=a->value?copy(a->value):NULL;
            out->attrs[i].kind=a->kind;
            if(!out->attrs[i].name || (a->value && !out->attrs[i].value))return CHML_ERR_MEMORY;
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
    if (doc->root_count >= (SIZE_MAX / sizeof(*doc->roots)) - 1) {free_node(&tmp);return CHML_ERR_MEMORY;}
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

/* ---- Encoding declaration policy ----
   CHML always writes UTF-8 bytes, so a document that carries a conflicting
   declaration is rejected before the destination is opened rather than emitting
   bytes its own header misdescribes. */
static int is_utf8_label(const char *v) {
    char buf[24]; size_t n=0;
    while(*v && is_ascii_ws((unsigned char)*v))v++;
    while(*v && !is_ascii_ws((unsigned char)*v) && *v!=';' && *v!='"' && *v!='\'') {
        if(n+1<sizeof buf) buf[n++]=(char)ascii_fold((unsigned char)*v);
        v++;
    }
    buf[n]=0;
    /* "utf-8" and "utf8" are both registered labels for UTF-8. */
    return strcmp(buf,"utf-8")==0 || strcmp(buf,"utf8")==0;
}
static const char *find_ci(const char *hay,const char *needle) {
    size_t n=strlen(needle);
    for(const char *p=hay;*p;p++) {
        size_t i=0;
        while(i<n && p[i] && ascii_fold((unsigned char)p[i])==ascii_fold((unsigned char)needle[i]))i++;
        if(i==n)return p+n;
    }
    return NULL;
}
static const char *attr_value(const struct ChmlNode *n,const char *name) {
    for(size_t i=0;i<n->attr_count;i++)
        if(ascii_ci_equal(n->attrs[i].name,name))return n->attrs[i].value;
    return NULL;
}
/* 0 = no declaration, 1 = declares UTF-8, -1 = declares something else. */
static int charset_state(const struct ChmlNode *n) {
    int state=0;
    if(n->tag && ascii_ci_equal(n->tag,"meta")) {
        const char *cs=attr_value(n,"charset");
        if(cs) return is_utf8_label(cs)?1:-1;
        const char *equiv=attr_value(n,"http-equiv");
        if(equiv && ascii_ci_equal(equiv,"content-type")) {
            const char *content=attr_value(n,"content");
            if(content) {
                const char *rest=find_ci(content,"charset=");
                if(rest) return is_utf8_label(rest)?1:-1;
            }
        }
    }
    for(size_t i=0;i<n->child_count;i++) {
        int s=charset_state(&n->children[i]);
        if(s<0)return -1;
        if(s>0)state=1;
    }
    return state;
}
static int has_head_in(const struct ChmlNode *n) {
    if(n->tag && ascii_ci_equal(n->tag,"head"))return 1;
    for(size_t i=0;i<n->child_count;i++)if(has_head_in(&n->children[i]))return 1;
    return 0;
}
static int has_head(const struct ChmlDocument *doc) {
    for(size_t i=0;i<doc->root_count;i++)
        if(has_head_in(&doc->roots[i]))return 1;
    return 0;
}

static int emit(FILE *f,const struct ChmlNode *n,int *inject_charset,int raw_text) {
    if(!n->tag) {
        if(!n->text)return 1;
        /* A raw-text parent's text children are emitted verbatim; see text_kind(). */
        return raw_text ? fputs(n->text,f)!=EOF : escaped(f,n->text,0);
    }
    if(fprintf(f,"<%s",n->tag)<0)return 0;
    for(size_t i=0;i<n->attr_count;i++) {
        const ChmlAttrDef *a=&n->attrs[i];
        if(a->kind==CHML_ATTR_BOOLEAN) {
            if(fprintf(f," %s",a->name)<0)return 0;
        } else if(fprintf(f," %s=\"",a->name)<0 || !escaped(f,a->value,1) || fputc('"',f)==EOF) {
            return 0;
        }
    }
    if(fputc('>',f)==EOF)return 0;
    if(*inject_charset && ascii_ci_equal(n->tag,"head")) {
        if(fputs("<meta charset=\"utf-8\">",f)==EOF)return 0;
        *inject_charset=0;
    }
    if(is_void(n->tag))return 1;
    ChmlTextKind kind=text_kind(n->tag);
    if(n->text) {
        if(kind==CHML_TEXT_RAW) {
            if(fputs(n->text,f)==EOF)return 0;
        } else if(!escaped(f,n->text,0)) {
            return 0;
        }
    }
    for(size_t i=0;i<n->child_count;i++)
        if(!emit(f,&n->children[i],inject_charset,kind==CHML_TEXT_RAW))return 0;
    return fprintf(f,"</%s>",n->tag)>=0;
}
ChmlStatus chml_document_write(const ChmlDocument *doc,const char *path) {
    if(!doc||!path)return CHML_ERR_ARGUMENT;
    int declared=0;
    for(size_t i=0;i<doc->root_count;i++) {
        int s=charset_state(&doc->roots[i]);
        if(s<0)return CHML_ERR_ARGUMENT;
        if(s>0)declared=1;
    }
    int inject=!declared && has_head(doc);
    FILE *f=fopen(path,"wb");if(!f)return CHML_ERR_IO;
    int ok=fputs("<!doctype html>\n",f)!=EOF;
    for(size_t i=0;ok&&i<doc->root_count;i++)ok=emit(f,&doc->roots[i],&inject,0);
    if(ok)ok=fputc('\n',f)!=EOF;
    if(fclose(f)!=0)ok=0;
    return ok?CHML_OK:CHML_ERR_IO;
}
void chml_document_destroy(ChmlDocument *doc) {
    if(!doc)return;
    for(size_t i=0;i<doc->root_count;i++)free_node(&doc->roots[i]);
    free(doc->roots);free(doc);
}
