/* Discriminates atomic replacement from in-place rewriting.
   Linked twice:
     - against the real library, it must exit 0 (identity changes);
     - against tests/support/chml_io_inplace.c, it must exit 1.
   The second is registered with WILL_FAIL, so a broken implementation that no
   longer replaces turns the suite red.

   A correct write followed by correct file content is NOT sufficient evidence:
   an in-place writer satisfies that too. Only the file identity distinguishes
   them. */
#include "chml/chml.h"
#include "chml_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static const char *PROBE = "atomic_identity_probe.html";

static int file_identity(const char *path, unsigned long long *id) {
#ifdef _WIN32
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BY_HANDLE_FILE_INFORMATION info;
    int got = GetFileInformationByHandle(h, &info) ? 1 : 0;
    CloseHandle(h);
    if (!got) return 0;
    *id = ((unsigned long long)info.nFileIndexHigh << 32) | info.nFileIndexLow;
    return 1;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    *id = ((unsigned long long)st.st_dev << 32) ^ (unsigned long long)st.st_ino;
    return 1;
#endif
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return NULL; }
    size_t got = fread(s, 1, (size_t)n, f);
    fclose(f);
    s[got] = 0;
    return s;
}

static int write_doc(const char *text) {
    ChmlDocument *d = chml_document_create();
    if (!d) return 0;
    ChmlNodeDef p = CHML_ELEMENT("p", CHML_TEXT(text));
    int ok = chml_document_add(d, &p) == CHML_OK &&
             chml_document_write(d, PROBE) == CHML_OK;
    chml_document_destroy(d);
    return ok;
}

int main(void) {
    remove(PROBE);
    printf("replace backend: %s\n", chml_replace_backend());

    unsigned long long id1 = 0, id2 = 0;
    if (!write_doc("first")) {
        puts("NEGATIVE_CONTROL_REASON=harness_error");
        puts("FAIL: first write");
        return 2;
    }
    if (!file_identity(PROBE, &id1)) {
        puts("NEGATIVE_CONTROL_REASON=harness_error");
        puts("FAIL: first identity");
        return 2;
    }

    if (!write_doc("second")) {
        puts("NEGATIVE_CONTROL_REASON=harness_error");
        puts("FAIL: second write");
        return 2;
    }
    if (!file_identity(PROBE, &id2)) {
        puts("NEGATIVE_CONTROL_REASON=harness_error");
        puts("FAIL: second identity");
        return 2;
    }

    char *content = read_all(PROBE);
    int content_ok = content && strstr(content, "<p>second</p>") != NULL;
    if (!content_ok) {
        puts("NEGATIVE_CONTROL_REASON=harness_error");
        printf("FAIL: content is %s\n", content ? content : "(null)");
        free(content);
        remove(PROBE);
        return 2;
    }
    free(content);
    remove(PROBE);

    /* The reason is printed so verification can distinguish the intended
       detection from a crash or an unrelated failure carrying the same exit
       status. Exit 1 means the intended condition; exit 2 means the harness
       itself broke. */
    if (id1 == id2) {
        puts("NEGATIVE_CONTROL_REASON=identity_unchanged");
        puts("RESULT: file identity unchanged -> rewritten in place (not atomic)");
        return 1;
    }
    puts("NEGATIVE_CONTROL_REASON=none");
    puts("RESULT: file identity changed -> replaced atomically");
    return 0;
}
