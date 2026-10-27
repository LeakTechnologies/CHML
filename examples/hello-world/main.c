#include "chml/chml.h"
#include "chml/style.h"
#include <stdio.h>
int main(void) {
    ChmlDocument *doc=chml_document_create();
    if(!doc)return 1;
    const ChmlAttrDef lang[]={CHML_ATTR("lang","en")};
    const ChmlAttrDef stylesheet[]={CHML_ATTR("rel","stylesheet"),CHML_ATTR("href","style.css")};
    const ChmlAttrDef viewport[]={CHML_ATTR("name","viewport"),CHML_ATTR("content","width=device-width, initial-scale=1")};
    const ChmlAttrDef shell[]={CHML_ATTR("class","shell")};
    const ChmlAttrDef hero[]={CHML_ATTR("class","hero")};
    const ChmlAttrDef grid[]={CHML_ATTR("class","projects"),CHML_ATTR("id","projects")};
    const ChmlAttrDef card[]={CHML_ATTR("class","card")};
    const ChmlAttrDef nav[]={CHML_ATTR("aria-label","Primary")};
    const ChmlAttrDef link[]={CHML_ATTR("href","#projects")};
    ChmlNodeDef page=CHML_WITH_ATTRS("html",lang,CHML_COUNT(lang),
        CHML_ELEMENT("head",
            CHML_ELEMENT("title",CHML_TEXT("CHML — A website in C")),
            CHML_LEAF_ATTRS("meta",viewport,CHML_COUNT(viewport)),
            CHML_LEAF_ATTRS("link",stylesheet,CHML_COUNT(stylesheet))
        ),
        CHML_ELEMENT("body",
            CHML_WITH_ATTRS("main",shell,CHML_COUNT(shell),
                CHML_WITH_ATTRS("nav",nav,CHML_COUNT(nav),
                    CHML_WITH_ATTRS("a",link,CHML_COUNT(link),CHML_TEXT("Projects"))
                ),
                CHML_WITH_ATTRS("header",hero,CHML_COUNT(hero),
                    CHML_ELEMENT("h1",CHML_TEXT("Hello from C.")),
                    CHML_ELEMENT("p",CHML_TEXT("No HTML source. No CSS source. Just C."))
                ),
                CHML_WITH_ATTRS("section",grid,CHML_COUNT(grid),
                    CHML_WITH_ATTRS("article",card,CHML_COUNT(card),
                        CHML_ELEMENT("h2",CHML_TEXT("Actual C")),
                        CHML_ELEMENT("p",CHML_TEXT("Compiled using an ordinary C compiler."))
                    ),
                    CHML_WITH_ATTRS("article",card,CHML_COUNT(card),
                        CHML_ELEMENT("h2",CHML_TEXT("Real browser")),
                        CHML_ELEMENT("p",CHML_TEXT("Standards-based output with no JavaScript."))
                    )
                ),
                CHML_ELEMENT("footer",CHML_ELEMENT("p",CHML_TEXT("This should not exist. Unfortunately, it works.")))
            )
        )
    );
    ChmlStatus st=chml_document_add(doc,&page);
    if(st==CHML_OK)st=chml_document_write(doc,"index.html");
    const ChmlDeclaration base[] = {
        CHML_DECL("color-scheme", "dark"), CHML_DECL("--accent", "#50d8c5")
    };
    const ChmlDeclaration universal[] = {CHML_DECL("box-sizing", "border-box")};
    const ChmlDeclaration body_style[] = {
        CHML_DECL("margin", "0"), CHML_DECL("background", "#10151c"),
        CHML_DECL("color", "#eef4f7"), CHML_DECL("font", "1rem/1.6 system-ui,sans-serif")
    };
    const ChmlDeclaration links[] = {CHML_DECL("color", "var(--accent)")};
    const ChmlDeclaration shell_style[] = {
        CHML_DECL("max-width", "1100px"), CHML_DECL("margin", "auto"),
        CHML_DECL("padding", "clamp(1.5rem,5vw,4rem)")
    };
    const ChmlDeclaration hero_style[] = {CHML_DECL("padding", "5rem 0 3rem")};
    const ChmlDeclaration heading_style[] = {
        CHML_DECL("font-size", "clamp(2.5rem,7vw,5rem)"), CHML_DECL("line-height", "1.1")
    };
    const ChmlDeclaration grid_style[] = {
        CHML_DECL("display", "grid"),
        CHML_DECL("grid-template-columns", "repeat(auto-fit,minmax(min(100%,260px),1fr))"),
        CHML_DECL("gap", "1rem")
    };
    const ChmlDeclaration card_style[] = {
        CHML_DECL("background", "#1a2430"), CHML_DECL("border", "1px solid #34404d"),
        CHML_DECL("border-radius", "14px"), CHML_DECL("padding", "1.5rem")
    };
    const ChmlDeclaration focus_style[] = {
        CHML_DECL("outline", "3px solid var(--accent)"), CHML_DECL("outline-offset", "4px")
    };
    const ChmlDeclaration transition_style[] = {CHML_DECL("transition", "color .15s")};
    const ChmlRuleDef rules[] = {
        CHML_RULE(":root",base), CHML_RULE("*",universal),
        CHML_RULE("body",body_style), CHML_RULE("a",links),
        CHML_RULE(".shell",shell_style), CHML_RULE(".hero",hero_style),
        CHML_RULE(".hero h1",heading_style), CHML_RULE(".projects",grid_style),
        CHML_RULE(".card",card_style), CHML_RULE("a:focus-visible",focus_style),
        CHML_MEDIA_RULE("(prefers-reduced-motion:no-preference)","a",transition_style)
    };
    const ChmlSheetDef sheet = {rules, CHML_COUNT(rules)};
    if(st==CHML_OK)st=chml_stylesheet_write(&sheet,"style.css");
    chml_document_destroy(doc);
    if(st!=CHML_OK){fprintf(stderr,"CHML error: %d\n",st);return 1;}
    puts("CHML build successful. Existential justification: None.");
    return 0;
}
