#include "Compiler.h"
#include "Token.h"

Location locationNew(u64 start, u64 end, FileID file) {
    return (Location){
        .start = start,
        .end = end,
        .file = file
    };
}
