/* Internal shared node representation.
   Both authoring front-ends - the declarative ChmlNodeDef path and the
   transactional builder - produce this tree and are serialized by the single
   implementation in document.c. There must never be a second serializer. */
#ifndef CHML_NODE_H
#define CHML_NODE_H
#include <stdio.h>
#include <stddef.h>
#include "chml/chml.h"

struct ChmlNode {
    char *tag, *text;
    ChmlAttrDef *attrs;
    size_t attr_count, attr_cap;
    struct ChmlNode **children;
    size_t child_count, child_cap;
};

/* Serializes a root list and replaces path atomically, applying the shared
   charset policy. Returns CHML_ERR_ARGUMENT for a conflicting declaration
   before the destination is opened. */
ChmlStatus chml_write_node_list(struct ChmlNode *const *roots, size_t count, const char *path);

/* Shared validation so both front-ends reject identical input. */
int        chml_name_valid(const char *s);
ChmlStatus chml_attr_validate(const ChmlAttrDef *a);

#endif
