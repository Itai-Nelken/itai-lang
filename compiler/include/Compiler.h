#ifndef COMPILER_H
#define COMPILER_H

#include <stdbool.h>
#include "common.h"
#include "Array.h"
#include "Strings.h"

// predeclarations for types (as Error.h includes this file, this file can't include Error.h)
typedef struct error Error; // from Error.h

// fileID contains an index into the Compiler::files array.
typedef usize FileID;

typedef struct file {
    String path;
    String contents;
} File;

/***
 * Initialize a new File.
 * 
 * @param f The file to initialize.
 * @param path The path.
 ***/
void fileInit(File *f, const char *path);

/***
 * Free a File.
 * 
 * @param f An initialized file to free.
 ***/
void fileFree(File *f);

/***
 * Get the basename of a File.
 * 
 * @param f The File.
 * @return The basename
 ***/
const char *fileBasename(File *f);

/***
 * Read the contents of a File.
 * NOTE: the contents are cached, meaning the file is read only on the first call.
 * 
 * @param f The File to read.
 * @return it's contents.
 ***/
String fileRead(File *f);


typedef struct compiler {
    Array files; // Array<File *>
    Array errors; // Array<Error *>
    FileID current_file;
    bool current_file_initialized; // if true current_file is valid, else it's invalid.
    String current_file_contents;
} Compiler;

/***
 * Initialize a Compiler.
 * 
 * @param c The Compiler to initialize.
 ***/
void compilerInit(Compiler *c);

/***
 * Free a Compiler.
 * 
 * @param c The Copmpiler to free.
 ***/
void compilerFree(Compiler *c);

/***
 * Add a file to the file list.
 * 
 * @param c The Compiler to add the file to.
 * @param path The path to the file.
 * @return The FileID of the added file.
 ***/
FileID compilerAddFile(Compiler *c, const char *path);

/***
 * Get a pointer to the File pointed to by a FileID.
 * 
 * @param c The compiler to get the File from.
 * @param id The FileID of the file.
 * @return A pointer to the File or NULL on error.
 ***/
File *compilerGetFile(Compiler *c, FileID id);

/***
 * Get a pointer to the current File.
 * 
 * @param c The compiler to get the File from.
 * @return A pointer to the current File or NULL on error.
 ***/
File *compilerGetCurrentFile(Compiler *c);

/***
 * Add an Error to a Compiler.
 * NOTE: ownership of the error is taken.
 * 
 * @param c The Compiler to add the Error to.
 * @param err The Error to add.
 ***/
void compilerAddError(Compiler *c, Error *err);

/***
 * Print all errors in a Compiler.
 * 
 * @param c A Compiler.
 ***/
void compilerPrintErrors(Compiler *c);

#endif // COMPILER_H
