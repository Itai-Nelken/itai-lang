#include "Types.h"

DataType dataTypeNew(SymbolID name_id, usize size, bool is_signed) {
    return (DataType){
        .name = name_id,
        .size = size,
        .is_signed = is_signed
    };
}

void dataTypePrint(FILE *to, DataType ty) {
    fprintf(to, "DataType{\x1b[1mname:\x1b[0m \x1b[34m%zu\x1b[0m", ty.name);
    fprintf(to, ", \x1b[1msize:\x1b[0m \x1b[34m%zu\x1b[0m", ty.size);
    fprintf(to, ", \x1b[1mis_signed:\x1b[0m %s}", ty.is_signed ? "true" : "false");
}
