#include "components.h"
#include <string.h>

ChmlStatus page_layout_begin(ChmlBuilder *b, const char *title, ChmlNode **main_node) {
    if (!b || !title || !main_node) return CHML_ERR_ARGUMENT;

    ChmlNode *html = NULL, *head = NULL, *link = NULL, *body = NULL, *main = NULL;
    ChmlStatus st = chml_builder_begin(b, "html", &html);
    if (st == CHML_OK) st = chml_builder_attr_string(b, html, "lang", "en");
    if (st == CHML_OK) st = chml_builder_element(b, html, "head", &head);
    if (st == CHML_OK) st = chml_builder_add_text(b, head, "title", title);
    if (st == CHML_OK) st = chml_builder_element(b, head, "link", &link);
    if (st == CHML_OK) st = chml_builder_attr_string(b, link, "rel", "stylesheet");
    if (st == CHML_OK) st = chml_builder_attr_string(b, link, "href", "style.css");
    if (st == CHML_OK) st = chml_builder_element(b, html, "body", &body);
    if (st == CHML_OK) st = chml_builder_element(b, body, "main", &main);
    if (st == CHML_OK) st = chml_builder_attr_string(b, main, "class", "shell");

    if (st != CHML_OK) {
        chml_builder_discard(b, html);   /* no-op when begin() itself failed */
        return st;
    }
    *main_node = main;
    return CHML_OK;
}

ChmlStatus page_layout_commit(ChmlBuilder *b) {
    return chml_builder_attach(b, NULL, NULL);
}

ChmlStatus navigation_add(ChmlBuilder *b, ChmlNode *parent,
                          const ChmlNavLink *links, size_t count) {
    if (!b || !parent || (!links && count)) return CHML_ERR_ARGUMENT;
    ChmlMark mark = chml_builder_mark(b, parent);

    ChmlNode *nav = NULL, *list = NULL;
    ChmlStatus st = chml_builder_element(b, parent, "nav", &nav);
    if (st == CHML_OK) st = chml_builder_attr_string(b, nav, "aria-label", "Primary");
    if (st == CHML_OK) st = chml_builder_element(b, nav, "ul", &list);

    for (size_t i = 0; st == CHML_OK && i < count; i++) {
        ChmlNode *li = NULL, *a = NULL;
        st = chml_builder_element(b, list, "li", &li);
        if (st == CHML_OK) st = chml_builder_element(b, li, "a", &a);
        if (st == CHML_OK) st = chml_builder_attr_string(b, a, "href", links[i].href);
        if (st == CHML_OK) st = chml_builder_text(b, a, links[i].label);
    }

    if (st != CHML_OK) { chml_builder_rollback(b, &mark); return st; }
    return CHML_OK;
}

ChmlStatus project_card_add(ChmlBuilder *b, ChmlNode *parent,
                            const char *title, const char *body, const char *href) {
    if (!b || !parent || !title || !body) return CHML_ERR_ARGUMENT;
    ChmlMark mark = chml_builder_mark(b, parent);

    ChmlNode *card = NULL;
    ChmlStatus st = chml_builder_element(b, parent, "article", &card);
    if (st == CHML_OK) st = chml_builder_attr_string(b, card, "class", "card");
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "h2", title);
    if (st == CHML_OK) st = chml_builder_add_text(b, card, "p", body);
    if (st == CHML_OK && href) {
        ChmlNode *p = NULL, *a = NULL;
        st = chml_builder_element(b, card, "p", &p);
        if (st == CHML_OK) st = chml_builder_element(b, p, "a", &a);
        if (st == CHML_OK) st = chml_builder_attr_string(b, a, "href", href);
        if (st == CHML_OK) st = chml_builder_text(b, a, "Read more");
    }

    if (st != CHML_OK) { chml_builder_rollback(b, &mark); return st; }
    return CHML_OK;
}
