/* M0.5-A acceptance example: a small site assembled from reusable C functions. */
#include "components.h"
#include "chml/style.h"
#include <stdio.h>

static const ChmlNavLink LINKS[] = {
    { "Projects", "#projects" },
    { "About",    "#about" },
};

int main(void) {
    ChmlBuilder *b = chml_builder_create();
    if (!b) { fputs("builder allocation failed\n", stderr); return 1; }

    ChmlNode *main = NULL;
    ChmlStatus st = page_layout_begin(b, "Leak Technologies", &main);
    if (st == CHML_OK) st = navigation_add(b, main, LINKS, CHML_COUNT(LINKS));
    if (st == CHML_OK) st = project_card_add(b, main, "VideoTools",
                                             "Media processing suite. A & B < C.", "#videotools");
    if (st == CHML_OK) st = project_card_add(b, main, "CHML",
                                             "The web, written in C.", "#chml");
    if (st == CHML_OK) st = page_layout_commit(b);
    if (st == CHML_OK) st = chml_builder_write(b, "index.html");

    const ChmlDeclaration base[] = { CHML_DECL("color-scheme", "dark") };
    const ChmlDeclaration shell[] = { CHML_DECL("max-width", "60rem"),
                                      CHML_DECL("margin", "0 auto") };
    const ChmlDeclaration card[] = { CHML_DECL("border", "1px solid #34404d"),
                                     CHML_DECL("padding", "1rem") };
    const ChmlRuleDef rules[] = {
        CHML_RULE(":root", base),
        CHML_RULE(".shell", shell),
        CHML_RULE(".card", card),
    };
    const ChmlSheetDef sheet = { rules, CHML_COUNT(rules) };
    if (st == CHML_OK) st = chml_stylesheet_write(&sheet, "style.css");

    chml_builder_destroy(b);
    if (st != CHML_OK) { fprintf(stderr, "CHML error: %d\n", (int)st); return 1; }
    puts("components example written");
    return 0;
}
