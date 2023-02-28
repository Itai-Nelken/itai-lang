#include <stdio.h>
//#include <stdlib.h> // labs()
#include <stdbool.h>
#include "common.h"
#include "memory.h"
#include "Compiler.h"
#include "Strings.h"
#include "Error.h"

void errorInit(Error *err, ErrorType type, bool has_location, Location location, const char *message) {
    err->type = type;
    err->has_location = has_location;
    err->location = location;
    err->message = stringCopy(message);
}

void errorFree(Error *err) {
    stringFree(err->message);
    err->message = NULL;
}

static const char *error_type_to_string(ErrorType type) {
    static const char *types[] = {
        [ERR_ERROR] = "\x1b[1;31mError\x1b[0m",
        [ERR_HINT]  = "\x1b[1;34mHint\x1b[0m"
    };
    return types[(i32)type];
}

static const char *error_type_color(ErrorType type) {
    static const char *colors[] = {
        [ERR_ERROR] = "\x1b[31m",
        [ERR_HINT]  = "\x1b[34m"
    };
    return colors[(i32)type];
}

struct line {
    usize start;
    usize end;
    u64 line_number;
    bool is_error_line;
};

struct line_array {
    struct line *lines;
    usize used, capacity;
};

static void push_line(struct line_array *lines, struct line line) {
    if(lines->used + 1 > lines->capacity || lines->lines == NULL) {
        // The initial capacity is 4 because in most errors there will be 3 lines:
        // * The line before the error.
        // * The line with the error.
        // * The line after the error.
        // 4 will store the 3 lines with a slot left over.
        lines->capacity = lines->capacity == 0 ? 4 : lines->capacity * 2;
        lines->lines = REALLOC(lines->lines, sizeof(*lines->lines) * lines->capacity);
    }
    lines->lines[lines->used++] = line;
}

static inline struct line *get_line(struct line_array *lines, usize idx) {
    return idx < lines->used ? &lines->lines[idx] : NULL;
}

static inline struct line *get_last_line(struct line_array *lines) {
    return get_line(lines, lines->used - 1);
}

static void collect_lines(String file_contents, Location loc, struct line_array *lines, struct line *first_error_line) {
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
            // set previous line (if exists).
            if(line_number > 1) {
                struct line before;
                before.is_error_line = false;
                before.start = prev_start;
                before.end = prev_end;
                before.line_number = line_number - 1;
                push_line(lines, before);
            }

            // set current line(s).
            struct line current = {0};
            current.is_error_line = true;
            current.start = current_start;
            // continue until end of the current line
            for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
            current.end = i;
            current.line_number = line_number;
            *first_error_line = current;
            push_line(lines, current);

            // set next line (if exists).
            if(i + 1 < stringLength(file_contents)) {
                struct line after;
                after.is_error_line = false;
                // 'i' points to the previous newline, so get the next character location.
                after.start = ++i;
                for(; i < stringLength(file_contents) && file_contents[i] != '\n'; ++i) /* nothing */;
                after.end = i;
                after.line_number = line_number + 1;
                push_line(lines, after);
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

/* FIXME:

The error created by the following output:
'''
1| fn test() -> i32
2|
'''
prints like this:
'''
Error: Expected '{' but got '<eof>'.
---- 1.ilc:3:0
 2 | 
 3 | 
     ^~ Expected '{' but got '<eof>'.
----
'''
which is wrong (there is no line 3).
*/

/* Error/Hint format:
Error/Hint: reason
---- file.ext:<line>:<char>
 1 |
 2 | fn test(abc) {
             ^~~ reason
 3 |     abc *= 2;
*/
void errorPrint(Error *err, Compiler *c, FILE *to) {
    if(!err->has_location) {
        fprintf(to, "%s: \x1b[1m%s\x1b[0m\n", error_type_to_string(err->type), err->message);
        return;
    }

    String file_contents = fileRead(compilerGetFile(c, err->location.file));
    if(file_contents == NULL) {
        LOG_ERR("Failed to read file '%s'!\n", compilerGetFile(c, err->location.file)->path);
        return;
    }
    struct line_array lines = {0};
    struct line first_error_line = {0};
    // get the contents of the line before, the line with, and the line after the error,
    collect_lines(file_contents, err->location, &lines, &first_error_line);
    // calculate the width of the largest line number.
    // if the line after isn't available, the current line number must be the largest.
    u32 largest_width = number_width(get_last_line(&lines)->line_number);

    fprintf(to, "%s: \x1b[1m%s\x1b[0m\n", error_type_to_string(err->type), err->message);
    fprintf(to, "---%*c \x1b[33m%s:%ld:%ld\x1b[0m\n", largest_width, '-', compilerGetFile(c, err->location.file)->path, first_error_line.line_number, err->location.start - first_error_line.start);
    
    // print all the lines
    for(usize i = 0; i < lines.used; ++i) {
        struct line *current_line = get_line(&lines, i);
        print_line(to, err, file_contents, current_line, largest_width);
        if(current_line->is_error_line) {
            // pad the line for " <line> | "
            fprintf(to, " %*c   ", largest_width, ' ');

            // if 'start' is larger than 0, pad until the offending character - 1.
            for(u64 i = current_line->start; i < err->location.start; ++i) {
                char c = ' ';
                if(file_contents[i] == '\t') {
                    c = '\t';
                }
                fputc(c, to);
            }
            fprintf(to, "%s\x1b[1m^-", error_type_color(err->type));
            //for(u64 i = 0; i < labs((isize)err->location.end - (isize)err->location.start - 1); ++i) {
            //    fputc('~', to);
            //}
            fprintf(to, "\x1b[0;1m %s\x1b[0m\n", err->message);
        }
    }
    fprintf(to, "---%*c\n", largest_width, '-');
    if(lines.lines) {
        FREE(lines.lines);
    }
}
