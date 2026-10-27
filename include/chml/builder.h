#ifndef CHML_BUILDER_H
#define CHML_BUILDER_H
/* Experimental transactional component builder (M0.5-A).
   Detached subtree construction, explicit attachment, stable handles.
   The API is provisional pending review; see docs/decisions/ADR-0005. */
#include "chml.h"

typedef struct ChmlBuilder ChmlBuilder;

/* A construction checkpoint inside the open session. */
typedef struct ChmlMark {
    void *block;
    size_t used;
    ChmlNode *parent;
    ChmlNode **children;
    size_t child_count;
    size_t child_cap;
    int valid;
} ChmlMark;

ChmlBuilder *chml_builder_create(void);
void         chml_builder_destroy(ChmlBuilder *b);

/* Opens a session and creates its detached root. One session at a time. */
ChmlStatus chml_builder_begin(ChmlBuilder *b, const char *tag, ChmlNode **root);

/* Construction. parent must belong to this builder (the open session, or an
   already attached document node). */
ChmlStatus chml_builder_element (ChmlBuilder *b, ChmlNode *parent, const char *tag, ChmlNode **out);
ChmlStatus chml_builder_text    (ChmlBuilder *b, ChmlNode *parent, const char *text);
ChmlStatus chml_builder_add_text(ChmlBuilder *b, ChmlNode *parent, const char *tag, const char *text);

ChmlStatus chml_builder_attr_string(ChmlBuilder *b, ChmlNode *node, const char *name, const char *value);
ChmlStatus chml_builder_attr_bool  (ChmlBuilder *b, ChmlNode *node, const char *name);

/* Per-component rollback inside the open session. */
ChmlMark chml_builder_mark  (ChmlBuilder *b, ChmlNode *parent);
void     chml_builder_rollback(ChmlBuilder *b, ChmlMark *mark);

/* Attaches the session root to parent, or to the document when parent is NULL.
   On success ownership moves to the document and handles stay valid. */
ChmlStatus chml_builder_attach(ChmlBuilder *b, ChmlNode *root, ChmlNode *parent);

/* Discards the open session; its handles become invalid. */
void chml_builder_discard(ChmlBuilder *b, ChmlNode *root);

ChmlStatus chml_builder_write(const ChmlBuilder *b, const char *path);

#endif
