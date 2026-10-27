#include "chml/style.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const ChmlDeclaration decls[] = {CHML_DECL("display","grid"),CHML_DECL("gap","1rem")};
    const ChmlRuleDef rules[] = {CHML_RULE(".grid",decls)};
    const ChmlSheetDef sheet={rules,1};
    assert(chml_stylesheet_write(&sheet,"style-test.css")==CHML_OK);
    FILE *f=fopen("style-test.css","rb");assert(f);
    char buf[128]={0};size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);
    assert(n==strlen(".grid{display:grid;gap:1rem;}\n"));
    assert(strcmp(buf,".grid{display:grid;gap:1rem;}\n")==0);
    const ChmlDeclaration invalid[] = {CHML_DECL("color","red;}body{display:none")};
    const ChmlRuleDef bad[] = {CHML_RULE(".x",invalid)};
    const ChmlSheetDef bad_sheet={bad,1};
    assert(chml_stylesheet_write(&bad_sheet,"should-not-exist.css")==CHML_ERR_ARGUMENT);
    remove("style-test.css");
    puts("style tests passed");return 0;
}
