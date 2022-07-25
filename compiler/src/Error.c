#include <stdio.h>
#include <stdbool.h>
#include "common.h"
#include "Compiler.h"
#include "Strings.h"
#include "Error.h"

void errorInit(Error *err, ErrorType type, Location location, const char *message) {
    err->type = type;
    err->location = location;
    err->message = stringCopy(message);
}

void errorFree(Error *err) {
    stringFree(err->message);
    err->message = NULL;
}

static const char *error_type_to_string(ErrorType type) {
    static const char *types[] = {
        [ERR_ERROR] = "\x1b[1;31mError\x1b[0m"
    };
    return types[(i32)type];
}

struct line {
    char *text;
    u32 length;
    u64 line_number;
    bool use;
};

static void collect_lines(String file_contents, Location loc, struct line *before, struct line *current, struct line *after) {
    u64 prev_start = 0, prev_end = 0, current_start = 0;
    u64 line_number = 1;
    for(usize i = 0; i < stringLength(file_contents); ++i) {
        if(file_contents[i] == '\n') {
            prev_start = current_start;
            prev_end = i;
            current_start = i;
            line_number++;
            continue;
        }
        // if the location is found, set all the line structs and return; 
        if(i == loc.start) {
            // set previous line.
            if(line_number > 1) {
                before->text = &file_contents[prev_start];
                before->length = prev_end - prev_start;
                before->line_number = line_number - 1;
                before->use = true;
            } else {
                before->use = false;
            }

            // set current line.
            current->text = &file_contents[current_start];
            for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
            current->length = i - current_start;
            current->line_number = line_number;
            current->use = true;

            // set next line.
            // 'i' points to the previous newline, so get the next character location.
            if(i + 1 < stringLength(file_contents)) {
                after->text = &file_contents[++i];
                current_start = i;
                for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
                after->length = i - current_start;
                after->line_number = line_number + 1;
                after->use = true;
            } else {
                after->use = false;
            }

            return;
        }
    }
}

/* 
Error: reason
---- file.ext:<line>:<char>
 1 |
 2 | fn test(abc) {
             ^~~ reason
 3 |     abc *= 2;
*/
void errorPrint(Error *err, Compiler *c, FILE *to) {
    struct line before = {0}, current = {0}, after = {0};
    String file_contents = fileRead(compilerGetFile(c, err->location.file));
    if(file_contents == NULL) {
        LOG_ERR_F("Failed to read file '%s'!\n", compilerGetFile(c, err->location.file)->path);
        return;
    }
    collect_lines(file_contents, err->location, &before, &current, &after);
    fprintf(to, "%s: %s\n", error_type_to_string(err->type), err->message);
    fprintf(to, "----- %s:%ld:%ld\n", fileBasename(compilerGetFile(c, err->location.file)), current.line_number, err->location.start);
    // line before
    if(before.use) {
        fprintf(to, " %ld | %.*s\n", before.line_number, before.length, before.text);
    }
    // line with error
    fprintf(to, " %ld | %.*s\n", current.line_number, current.length, current.text);
    // line with pointer to wrong character(s) and error message
    fprintf(to, "     %*c", (u32)err->location.start, ' ');
    fprintf(to, "^%*s %s\n", (u32)(err->location.end - err->location.start), "~", err->message);
    // line after
    if(after.use) {
        fprintf(to, " %ld | %.*s\n", after.line_number, after.length, after.text);
    }
    fprintf(to, "-----\n");
}
