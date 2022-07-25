#include <stdio.h>
#include <string.h>
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
    return (const char *)(p ? p + 1 : p);
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

    String buffer = stringNew(length + 1);
    if(fread(buffer, sizeof(char), length, fp) != length) {
        return NULL;
    }
    fclose(fp);
    f->contents = stringNew(length + 1);
    stringAppend(f->contents, "%s", buffer);
    return f->contents;
}

void compilerInit(Compiler *c) {
    c->current_file_initialized = false;
    c->current_file = 0; // 0 is a valid FileID, but initialize with it so current_file is a known value.
    arrayInit(&c->files);
    arrayInit(&c->errors);
    c->current_file_contents = NULL;
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
    if(c->current_file_contents) {
        stringFree(c->current_file_contents);
    }
}

FileID compilerAddFile(Compiler *c, const char *path) {
    File *f;
    NEW0(f);
    fileInit(f, path);
    return (FileID)arrayPush(&c->files, (void *)f);
}

File *compilerGetFile(Compiler *c, FileID id) {
    // arrayGet() will return NULL if the index is out of the array bounds.
    return ARRAY_GET_AS(File *, &c->files, (int)id);
}

File *compilerGetCurrentFile(Compiler *c) {
    if(!c->current_file_initialized) {
        return NULL;
    }
    return compilerGetFile(c, c->current_file);
}

void compilerAddError(Compiler *c, Error *err) {
    arrayPush(&c->errors, (void *)err);
}

void compilerPrintErrors(Compiler *c) {
    for(usize i = 0; i < c->errors.used; ++i) {
        errorPrint(ARRAY_GET_AS(Error *, &c->errors, i), c, stderr);
    }
}
