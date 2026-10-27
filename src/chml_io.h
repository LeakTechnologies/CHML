#ifndef CHML_IO_H
#define CHML_IO_H

#include <stdio.h>
#include "chml/chml.h"

/* Serializes a document or stylesheet into an already-open temporary file.
   Returns 1 on success, 0 on any write failure. */
typedef int (*ChmlFileWriter)(FILE *file, const void *ctx);

/* Writes path atomically: content goes to a temporary file beside the
   destination, which only replaces path after a complete, flushed, closed
   write. On failure the temporary file is removed and any existing path is
   left untouched. Returns CHML_ERR_ARGUMENT for a NULL argument, CHML_ERR_IO
   for any filesystem failure. */
ChmlStatus chml_write_file_atomic(const char *path, ChmlFileWriter writer, const void *ctx);

/* Identifies the compiled replacement mechanism at run time: "win32-movefileex"
   or "posix-rename". Verification uses this to prove the POSIX branch actually
   executed, because compiling it is not evidence that it ran. */
const char *chml_replace_backend(void);

#endif
