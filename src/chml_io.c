#include "chml_io.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

/* Number of temporary-name candidates to try before giving up. A collision only
   happens when a stale temporary file from another process exists. */
#define CHML_TEMP_ATTEMPTS 64

/* Replaces `to` with `from`. rename() from ISO C does not replace an existing
   destination on Windows, so the Win32 call is used there. Both paths leave the
   destination untouched when they fail, which is what atomicity depends on. */
static int replace_file(const char *from, const char *to) {
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

const char *chml_replace_backend(void) {
#ifdef _WIN32
    return "win32-movefileex";
#else
    return "posix-rename";
#endif
}

ChmlStatus chml_write_file_atomic(const char *path, ChmlFileWriter writer, const void *ctx) {
    if (!path || !writer) return CHML_ERR_ARGUMENT;

    size_t name_size = strlen(path) + 32;
    char *tmp = malloc(name_size);
    if (!tmp) return CHML_ERR_MEMORY;

    /* "x" is the C11 exclusive-create mode: it fails rather than truncating an
       existing file, so a stale temporary is never silently reused. */
    FILE *f = NULL;
    for (unsigned attempt = 0; attempt < CHML_TEMP_ATTEMPTS; attempt++) {
        if (snprintf(tmp, name_size, "%s.tmp%u", path, attempt) < 0) {
            free(tmp);
            return CHML_ERR_IO;
        }
        f = fopen(tmp, "wbx");
        if (f) break;
    }
    if (!f) { free(tmp); return CHML_ERR_IO; }

    int ok = writer(f, ctx);
    if (ok && fflush(f) != 0) ok = 0;
    /* fclose must run on every path, including the failure paths above. */
    if (fclose(f) != 0) ok = 0;

    if (!ok) {
        remove(tmp);
        free(tmp);
        return CHML_ERR_IO;
    }
    if (replace_file(tmp, path) != 0) {
        remove(tmp);
        free(tmp);
        return CHML_ERR_IO;
    }
    free(tmp);
    return CHML_OK;
}
