#ifndef TOKEN_H
#define TOKEN_H

#include "common.h"
#include "Compiler.h"

typedef struct location {
    u64 start, end;
    FileID file;
} Location;

Location locationNew(u64 start, u64 end, FileID file);

#endif // TOKEN_H
