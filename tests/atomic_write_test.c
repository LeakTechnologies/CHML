/* M0.4-B: atomic output.
   Verifies that a failed write never replaces or truncates an existing artifact,
   that successful writes truncate cleanly, and that no temporary file survives
   either outcome. */
#include "chml/chml.h"
#include "chml/style.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

static int failures = 0;
#define PREFIX "<!doctype html>\n"

static void ok(const char *label) { printf("  ok   %s\n", label); }
static void bad(const char *label, const char *detail) {
    failures++;
    printf("  FAIL %s%s%s\n", label, detail ? ": " : "", detail ? detail : "");
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static char *read_file(const char *path) {
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

/* The writer names temporaries "<path>.tmp<attempt>". Check the low attempts,
   which is what a fresh run would use. */
static int temp_residue(const char *path) {
    char name[512];
    for (unsigned i = 0; i < 8; i++) {
        snprintf(name, sizeof name, "%s.tmp%u", path, i);
        if (file_exists(name)) { remove(name); return 1; }
    }
    return 0;
}

static ChmlDocument *doc_with_text(const char *text) {
    ChmlDocument *d = chml_document_create();
    if (!d) return NULL;
    ChmlNodeDef p = CHML_ELEMENT("p", CHML_TEXT(text));
    if (chml_document_add(d, &p) != CHML_OK) { chml_document_destroy(d); return NULL; }
    return d;
}

/* Compares the file against PREFIX + body + "\n". */
static int expect_file(const char *path, const char *body) {
    char *got = read_file(path);
    size_t need = strlen(PREFIX) + strlen(body) + 2;
    char *want = malloc(need);
    if (want) snprintf(want, need, "%s%s\n", PREFIX, body);
    int same = got && want && strcmp(got, want) == 0;
    if (!same && got) printf("    got : %s\n", got);
    free(got); free(want);
    return same;
}

/* Returns a filesystem identity for path. Writing a file in place keeps the
   identity; replacing it via a rename installs a different file object. This is
   what distinguishes atomic replacement from in-place truncation. */
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

int main(void) {
    remove("atomic_out.html");
    remove("atomic_ro.html");
    remove("atomic_style.css");

    /* ---- 1. successful create ---- */
    {
        ChmlDocument *d = doc_with_text("alpha");
        if (!d) { bad("create document", NULL); return 1; }
        if (chml_document_write(d, "atomic_out.html") != CHML_OK) bad("write new file", NULL);
        else if (!expect_file("atomic_out.html", "<p>alpha</p>")) bad("new file content", NULL);
        else if (temp_residue("atomic_out.html")) bad("no temporary left after create", NULL);
        else ok("new file is written whole, with no temporary left behind");
        chml_document_destroy(d);
    }

    /* ---- 2. overwrite truncates: a shorter document must not leave a tail ---- */
    {
        ChmlDocument *d = doc_with_text("x");
        if (!d) { bad("overwrite document", NULL); }
        else {
            if (chml_document_write(d, "atomic_out.html") != CHML_OK) bad("overwrite file", NULL);
            else if (!expect_file("atomic_out.html", "<p>x</p>")) bad("overwrite truncates previous content", NULL);
            else ok("overwrite replaces the previous content completely");
            chml_document_destroy(d);
        }
    }

    /* ---- 3. a missing directory fails and creates nothing ---- */
    {
        ChmlDocument *d = doc_with_text("x");
        if (!d) { bad("missing dir document", NULL); }
        else {
            ChmlStatus st = chml_document_write(d, "chml_no_such_dir_xyz/out.html");
            if (st != CHML_ERR_IO) bad("missing directory reports CHML_ERR_IO", NULL);
            else if (file_exists("chml_no_such_dir_xyz")) bad("missing directory is not created", NULL);
            else ok("a missing destination directory fails cleanly");
            chml_document_destroy(d);
        }
    }

    /* ---- 4. destination is a directory: replace fails, temp is cleaned up ---- */
    {
        ChmlDocument *d = doc_with_text("x");
        if (!d) { bad("dir destination document", NULL); }
        else {
            ChmlStatus st = chml_document_write(d, ".");
            if (st != CHML_ERR_IO) bad("directory destination reports CHML_ERR_IO", NULL);
            else if (temp_residue(".")) bad("temp is removed when replace fails", NULL);
            else ok("a failed replace removes its temporary file");
            chml_document_destroy(d);
        }
    }

    /* ---- 5. an existing artifact survives a failed replacement ---- */
#if defined(_WIN32)
    {
        ChmlDocument *keep = doc_with_text("ORIGINAL");
        ChmlDocument *next = doc_with_text("REPLACEMENT");
        if (!keep || !next) { bad("read-only fixture", NULL); }
        else if (chml_document_write(keep, "atomic_ro.html") != CHML_OK) {
            bad("read-only fixture write", NULL);
        } else {
            _chmod("atomic_ro.html", _S_IREAD);
            ChmlStatus st = chml_document_write(next, "atomic_ro.html");
            if (st != CHML_ERR_IO) bad("read-only destination reports CHML_ERR_IO", NULL);
            else if (!expect_file("atomic_ro.html", "<p>ORIGINAL</p>"))
                bad("read-only destination keeps its original content", NULL);
            else if (temp_residue("atomic_ro.html")) bad("no temporary left after failed replace", NULL);
            else ok("a failed replacement leaves the existing artifact intact");
            _chmod("atomic_ro.html", _S_IWRITE);
            remove("atomic_ro.html");
        }
        chml_document_destroy(keep);
        chml_document_destroy(next);
    }
#else
    puts("  SKIP read-only replacement guard (POSIX rename ignores target mode)");
#endif

    /* ---- 6. the stylesheet writer uses the same path ---- */
    {
        const ChmlDeclaration decls[] = {CHML_DECL("display", "grid")};
        const ChmlRuleDef rules[] = {CHML_RULE(".a", decls)};
        const ChmlSheetDef sheet = {rules, 1};
        if (chml_stylesheet_write(&sheet, "atomic_style.css") != CHML_OK) bad("stylesheet write", NULL);
        else if (temp_residue("atomic_style.css")) bad("stylesheet leaves no temporary", NULL);
        else ok("stylesheet output is written atomically too");

        if (chml_stylesheet_write(&sheet, "chml_no_such_dir_xyz/out.css") != CHML_ERR_IO)
            bad("stylesheet to a missing directory reports CHML_ERR_IO", NULL);
        else ok("stylesheet write fails cleanly on a missing directory");
    }

    /* ---- 7. the artifact is replaced, not rewritten in place ---- */
    {
        remove("atomic_id.html");
        ChmlDocument *first = doc_with_text("first");
        ChmlDocument *second = doc_with_text("second");
        unsigned long long id1 = 0, id2 = 0;
        if (!first || !second) { bad("identity fixture", NULL); }
        else if (chml_document_write(first, "atomic_id.html") != CHML_OK) bad("identity first write", NULL);
        else if (!file_identity("atomic_id.html", &id1)) bad("read file identity", NULL);
        else if (chml_document_write(second, "atomic_id.html") != CHML_OK) bad("identity second write", NULL);
        else if (!file_identity("atomic_id.html", &id2)) bad("read second file identity", NULL);
        else if (!expect_file("atomic_id.html", "<p>second</p>")) bad("identity final content", NULL);
        else if (id1 == id2) bad("rewrite is a replacement, not in-place truncation", NULL);
        else ok("an existing artifact is replaced by a different file object");
        chml_document_destroy(first);
        chml_document_destroy(second);
        remove("atomic_id.html");
    }

    remove("atomic_out.html");
    remove("atomic_style.css");
    if (failures) { printf("%d atomic write check(s) failed\n", failures); return 1; }
    puts("atomic write checks passed");
    return 0;
}
