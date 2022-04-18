#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>
#include "ast.h"

/***
 * Validate an ASTProg.
 * 
 * @param prog An initialized ASTProg containing a parsed program.
 * @return true if successful, false if not.
 ***/
bool validate(ASTProg *prog);

#endif // VALIDATOR_H
