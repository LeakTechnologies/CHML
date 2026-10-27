/* Internal test seam for the builder. Not part of the proposed public API. */
#ifndef CHML_BUILDER_INTERNAL_H
#define CHML_BUILDER_INTERNAL_H
#include <stddef.h>
#include "chml/builder.h"

/* Replaces the block allocator so tests can fail the Nth allocation.
   Passing NULL restores the default allocator. */
typedef void *(*ChmlTestAlloc)(void *ctx, size_t n);
typedef void  (*ChmlTestFree)(void *ctx, void *p);
void chml_builder_test_set_allocator(ChmlBuilder *b, ChmlTestAlloc alloc, ChmlTestFree release, void *ctx);

/* Bytes currently held by this builder, for leak and retention checks. */
size_t chml_builder_test_bytes(const ChmlBuilder *b);

/* Forces a block size so that every allocation reaches the allocator. Must be
   called before any construction; used to make failure points addressable. */
void chml_builder_test_set_block_size(ChmlBuilder *b, size_t block_size);

#endif
