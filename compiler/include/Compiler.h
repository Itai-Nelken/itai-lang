#ifndef COMPILER_H
#define COMPILER_H

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

typedef struct compiler {
    Array files; // Array<File *>
    Array errors; // Array<Error *>
    FileID current_file;
    bool current_file_initialized; // if true current_file is valid, else it's invalid.
} Compiler;


/** File **/

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
 * NOTE: The contents are cached, meaning the file is read only on the first call.
 *       This also means that you should NOT free the returned String.
 *
 * @param f The File to read.
 * @return it's contents.
 ***/
String fileRead(File *f);

/***
 * Check if a file exists in a base directory.
 * The path may include directories.
 * For example: to check if the file "a/b/c.txt" exists in "current" directory, baseDir will ".".
 *
 * @param baseDir The base directory to search from.
 * @param path The path to the file to check.
 * @return true if the file exists, false if it doesn't.
 ***/
bool doesFileExist(const char *baseDir, const char *path);

/** Compiler **/

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
 * Check if there is another file in the files array after the current file.
 * 
 * @param c A Compiler.
 * @return true if there is a next file, false if not.
 ***/
bool compilerHasNextFile(Compiler *c);

/***
 * Update the current file to the next available file and return it's FileID.
 *
 * @param c The compiler to get the File from.
 * @return The FileID of the next file.
 ***/
FileID compilerNextFile(Compiler *c);

/***
 * Get a pointer to the File pointed to by a FileID.
 *
 * @param c The compiler to get the File from.
 * @param id The FileID of the file.
 * @return A pointer to the File or NULL on error.
 ***/
File *compilerGetFile(Compiler *c, FileID id);

/***
 * Return the FileID of the current file.
 *
 * @param c The compiler to get the current FileID from.
 * @return The FileID of the current file.
 ***/
FileID compilerGetCurrentFileID(Compiler *c);

/***
 * Return the FileID of the first file.
 *
 * @param c The compiler to get the first FileID from.
 * @return The FileID of the first file.
 ***/
FileID compilerGetFirstFileID(Compiler *c);

/***
 * Add an Error to a Compiler.
 * NOTE: ownership of the error is taken.
 *
 * @param c The Compiler to add the Error to.
 * @param err The Error to add.
 ***/
void compilerAddError(Compiler *c, Error *err);

/***
 * Check if the compiler has any errors.
 * 
 * @param c A Compiler.
 * @return true if any error was added, false if not.
 ***/
bool compilerHadError(Compiler *c);

/***
 * Print all errors in a Compiler.
 *
 * @param c A Compiler.
 ***/
void compilerPrintErrors(Compiler *c);

#endif // COMPILER_H
