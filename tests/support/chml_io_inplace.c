/* DELIBERATELY NON-ATOMIC. Test fixture, not production code.
   Implements the same chml_write_file_atomic contract by writing straight to the
   destination, so that a test which only checks "the file has the right content"
   cannot tell the difference. It exists so the atomic-identity test can be linked
   against a broken implementation and must then fail.

   It is built only into the `chml_atomic_identity_negative` test target and is
   never part of the chml library. If this file is ever linked into production,
   atomic output is silently disabled. */
#include "chml_io.h"
#include <stdio.h>

ChmlStatus chml_write_file_atomic(const char *path, ChmlFileWriter writer, const void *ctx) {
    if (!path || !writer) return CHML_ERR_ARGUMENT;
    FILE *f = fopen(path, "wb");   /* truncates the destination immediately */
    if (!f) return CHML_ERR_IO;
    int ok = writer(f, ctx);
    if (ok && fflush(f) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? CHML_OK : CHML_ERR_IO;
}

const char *chml_replace_backend(void) { return "in-place-write (deliberately non-atomic)"; }
