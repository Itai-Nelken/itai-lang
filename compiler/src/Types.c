#include "Types.h"

Type newType(TypeType type, Location loc) {
    return (Type){
        .type = type,
        .location = loc
    };
}
