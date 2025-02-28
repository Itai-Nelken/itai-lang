#include <stdio.h>
#include <string.h>
#include <unistd.h> // access()
#include "common.h"
#include "memory.h"
#include "Strings.h"
#include "Error.h"
#include "Compiler.h"

void fileInit(File *f, const char *path) {
    f->path = stringCopy(path);
    f->contents = NULL;
}

void fileFree(File *f) {
    stringFree(f->path);
    f->path = NULL;
    if(f->contents) {
        stringFree(f->contents);
        f->contents = NULL;
    }
}

const char *fileBasename(File *f) {
    char *p = strrchr(f->path, '/');
    return (const char *)(p ? p + 1 : f->path);
}

bool doesFileExist(const char *baseDir, const char *path) {
    String fullPath = stringFormat("%s/%s", baseDir, path);
    bool result = access(fullPath, F_OK) == 0; // TODO: I don't think this is portable.
    stringFree(fullPath);
    return result;
}

String fileRead(File *f) {
    if(f->contents) {
        return f->contents;
    }
    // FIXME: this isn't portable.
    //        SEEK_END is optional according to the C stdlib spec.
    usize length = 0;
    FILE *fp = fopen(f->path, "r");
    if(!fp) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    rewind(fp);

    // A buffer is used instead of directly writing to 'f.contents'
    // because fread doesn't set the length of the string.
    String buffer = stringNew(length + 1);
    if(fread(buffer, sizeof(char), length, fp) != length) {
        stringFree(buffer);
        return NULL;
    }
    fclose(fp);
    buffer[length] = '\0';
    // can't use stringDuplicate because 'buffer' has no length set.
    f->contents = stringNCopy((const char *)buffer, length + 1);
    stringFree(buffer);
    return f->contents;
}

void compilerInit(Compiler *c) {
    c->current_file_initialized = false;
    c->current_file = 0; // 0 is a valid FileID, but initialize with it so current_file is a known value.
    arrayInit(&c->files);
    arrayInit(&c->errors);
}

static void free_file_callback(void *f, void *cl) {
    UNUSED(cl);
    fileFree((File *)f);
    FREE(f);
}

static void free_error_callback(void *err, void *cl) {
    UNUSED(cl);
    errorFree((Error *)err);
    FREE(err);
}

void compilerFree(Compiler *c) {
    c->current_file_initialized = false;
    c->current_file = 0; // see comment in compilerInit().
    arrayMap(&c->files, free_file_callback, NULL);
    arrayFree(&c->files);
    arrayMap(&c->errors, free_error_callback, NULL);
    arrayFree(&c->errors);
}

FileID compilerAddFile(Compiler *c, const char *path) {
    File *f;
    NEW0(f);
    fileInit(f, path);
    return (FileID)arrayPush(&c->files, (void *)f);
}

bool compilerHasNextFile(Compiler *c) {
    // If the current file is known, check that there is one after it,
    // otherwise check if there are any files at all.
    if(c->current_file_initialized) {
        return c->current_file + 1 < c->files.used;
    } else {
        return c->files.used > 0;
    }
}

FileID compilerNextFile(Compiler *c) {
    VERIFY(compilerHasNextFile(c));
    if(!c->current_file_initialized) {
        c->current_file_initialized = true;
        return c->current_file;
    }
    c->current_file = (FileID)(c->current_file + 1);
    return c->current_file;
}

File *compilerGetFile(Compiler *c, FileID id) {
    // arrayGet() will return NULL if the index is out of the array bounds.
    return ARRAY_GET_AS(File *, &c->files, (int)id);
}

FileID compilerGetCurrentFileID(Compiler *c) {
    VERIFY(c->current_file_initialized);
    return c->current_file;
}

FileID compilerGetFirstFileID(Compiler *c) {
    UNUSED(c);
    return (FileID)0;
}

void compilerAddError(Compiler *c, Error *err) {
    arrayPush(&c->errors, (void *)err);
}

bool compilerHadError(Compiler *c) {
    return c->errors.used > 0;
}

void compilerPrintErrors(Compiler *c) {
    for(usize i = 0; i < c->errors.used; ++i) {
        errorPrint(ARRAY_GET_AS(Error *, &c->errors, i), c, stderr);
    }
}
