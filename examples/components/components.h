/* Reference components for M0.5-A. Ordinary ISO C11 functions over the
   experimental builder. Each takes a mark and rolls back on failure, so a
   failed component leaves the parent exactly as it was. */
#ifndef CHML_COMPONENTS_H
#define CHML_COMPONENTS_H
#include "chml/builder.h"

typedef struct { const char *label; const char *href; } ChmlNavLink;

/* Opens a session and builds <html> with head and body>main. Returns <main>. */
ChmlStatus page_layout_begin(ChmlBuilder *b, const char *title, ChmlNode **main_node);

/* Attaches the page root built by page_layout_begin to the document. */
ChmlStatus page_layout_commit(ChmlBuilder *b);

ChmlStatus navigation_add(ChmlBuilder *b, ChmlNode *parent,
                          const ChmlNavLink *links, size_t count);

ChmlStatus project_card_add(ChmlBuilder *b, ChmlNode *parent,
                            const char *title, const char *body, const char *href);

#endif
