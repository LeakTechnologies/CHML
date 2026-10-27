#include "chml/chml.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f=fopen(path,"rb"); assert(f);
    assert(fseek(f,0,SEEK_END)==0);
    long n=ftell(f); assert(n>=0);
    assert(fseek(f,0,SEEK_SET)==0);
    char *s=malloc((size_t)n+1); assert(s);
    assert(fread(s,1,(size_t)n,f)==(size_t)n);
    s[n]=0; assert(fclose(f)==0); return s;
}
int main(void) {
    ChmlDocument *d=chml_document_create(); assert(d);
    ChmlNodeDef bad=CHML_ELEMENT("img",CHML_TEXT("illegal"));
    assert(chml_document_add(d,&bad)==CHML_ERR_ARGUMENT);
    ChmlNodeDef invalid=CHML_LEAF("1bad");
    assert(chml_document_add(d,&invalid)==CHML_ERR_ARGUMENT);
    /* HTML names are ASCII case-insensitive: uppercase void elements must be
       rejected too, otherwise <IMG>child</IMG> reaches the browser. */
    ChmlNodeDef upper_void=CHML_ELEMENT("IMG",CHML_TEXT("illegal"));
    assert(chml_document_add(d,&upper_void)==CHML_ERR_ARGUMENT);
    ChmlNodeDef mixed_void=CHML_ELEMENT("Img",CHML_LEAF("b"));
    assert(chml_document_add(d,&mixed_void)==CHML_ERR_ARGUMENT);
    /* Duplicate attributes are dropped by the parser and lose author data. */
    const ChmlAttrDef dup[]={CHML_ATTR("class","a"),CHML_ATTR("class","b")};
    ChmlNodeDef dup_attrs=CHML_LEAF_ATTRS("p",dup,CHML_COUNT(dup));
    assert(chml_document_add(d,&dup_attrs)==CHML_ERR_ARGUMENT);
    const ChmlAttrDef dup_case[]={CHML_ATTR("class","a"),CHML_ATTR("CLASS","b")};
    ChmlNodeDef dup_case_attrs=CHML_LEAF_ATTRS("p",dup_case,CHML_COUNT(dup_case));
    assert(chml_document_add(d,&dup_case_attrs)==CHML_ERR_ARGUMENT);
    /* Rejected additions must leave the document unchanged. */
    const ChmlAttrDef attrs[]={CHML_ATTR("title","A & \"quote\" <tag>")};
    ChmlNodeDef valid=CHML_WITH_ATTRS("p",attrs,CHML_COUNT(attrs),CHML_TEXT("A & B < C"));
    assert(chml_document_add(d,&valid)==CHML_OK);
    assert(chml_document_write(d,"chml_document_test.html")==CHML_OK);
    char *html=read_all("chml_document_test.html");
    assert(strstr(html,"title=\"A &amp; &quot;quote&quot; &lt;tag&gt;\"")!=NULL);
    assert(strstr(html,"A &amp; B &lt; C")!=NULL);
    /* Only the one valid root may appear: rejected adds leave no residue. */
    size_t roots=0;
    for(const char *s=html;(s=strstr(s,"<p"))!=NULL;s++)roots++;
    assert(roots==1);
    assert(strstr(html,"<IMG")==NULL);
    assert(strstr(html,"<Img")==NULL);
    assert(strstr(html,"class=\"a\"")==NULL);
    free(html); remove("chml_document_test.html");
    chml_document_destroy(d); puts("document test passed"); return 0;
}
