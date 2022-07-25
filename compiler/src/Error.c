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

static const char *error_type_color(ErrorType type) {
    static const char *colors[] = {
        [ERR_ERROR] = "\x1b[31m"
    };
    return colors[(i32)type];
}

struct line {
    usize start;
    usize end;
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
            current_start = i + 1; // +1 to skip the newline.
            line_number++;
            continue;
        }
        // if the location is found, set all the line structs and return; 
        if(i == loc.start) {
            // set previous line.
            if(line_number > 1) {
                before->start = prev_start;
                before->end = prev_end;
                before->line_number = line_number - 1;
                before->use = true;
            } else {
                before->use = false;
            }

            // set current line.
            current->start = current_start;
            for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
            current->end = i;
            current->line_number = line_number;
            current->use = true;

            // set next line.
            if(i + 1 < stringLength(file_contents)) {
                // 'i' points to the previous newline, so get the next character location.
                after->start = ++i;
                for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
                after->end = i;
                after->line_number = line_number + 1;
                after->use = true;
            } else {
                after->use = false;
            }

            return;
        }
    }
}

static u32 number_width(u32 value) {
    u32 width = 1;
    while(value > 9) {
        width++;
        value /= 10;
    }
    return width;
}

static void print_line(FILE *to, Error *err, String contents, struct line *line, u32 largest_width) {
    fprintf(to, " %*ld | ", largest_width, line->line_number);

    for(usize i = line->start; i < line->end; ++i) {
        char c = contents[i];
        if(i == err->location.start) {
            fprintf(to, "%s", error_type_color(err->type));
        }
        // not an else if in case start and end are the same for some reason.
        if(i == err->location.end) {
            fprintf(to, "\x1b[0m");
        }

        fputc(c, to);
    }
    fputc('\n', to);
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
    // get the contents of the line before, the line with, and the line after the error,
    collect_lines(file_contents, err->location, &before, &current, &after);
    // calculate the width of the largest line number.
    // if the line after isn't available, the current line number must be the largest.
    u32 largest_width = number_width(after.use ? after.line_number : current.line_number);

    fprintf(to, "%s: \x1b[1m%s\x1b[0m\n", error_type_to_string(err->type), err->message);
    fprintf(to, "---%*c \x1b[33m%s:%ld:%ld\x1b[0m\n", largest_width, '-', compilerGetFile(c, err->location.file)->path, current.line_number, err->location.start);
    
    // line before
    if(before.use) {
        print_line(to, err, file_contents, &before, largest_width);
    }

    // line with error
    print_line(to, err, file_contents, &current, largest_width);

    // line pointing to the error.
    // pad the line for " <line> | "
    fprintf(to, " %*c   ", largest_width, ' ');
    // if 'start' is larger than 0, pad until the offending character - 1.
    if(err->location.start > 0 && err->location.start != current.start) {
        fprintf(to, "%*c", (u32)(current.start - err->location.start - 1), ' ');
    }
    // print the '^' under the first offending character
    // followed by '~' for the rest (if they exist).
    fprintf(to, "\x1b[35;1m^");
    for(u64 i = 0; i < err->location.end - err->location.start - 1; ++i) {
        fputc('~', to);
    }
    fprintf(to, "\x1b[0;1m %s\x1b[0m\n", err->message);

    // line after
    if(after.use) {
        print_line(to, err, file_contents, &after, largest_width);
    }

    fprintf(to, "---%*c\n", largest_width, '-');
}
