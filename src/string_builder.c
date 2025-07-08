#include "string_builder.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

void string_builder_append_f(String_Builder *string_builder, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    bassert(len >= 0);

    char *chunk = malloc(len + 1);
    vsnprintf(chunk, len + 1, fmt, args);
    va_end(args);

    string_builder->chunk_count++;
    string_builder->chunks = xrealloc(string_builder->chunks, string_builder->chunk_count * sizeof(string_builder->chunks[0]));
    string_builder->chunks[string_builder->chunk_count - 1] = chunk;
}

void string_builder_append_str_range(String_Builder *string_builder, const char *str, int start, int count)
{
    string_builder->chunk_count++;
    string_builder->chunks = xrealloc(string_builder->chunks, string_builder->chunk_count * sizeof(string_builder->chunks[0]));
    string_builder->chunks[string_builder->chunk_count - 1] = xstrndup(str + start, count);
}

char *string_builder_compile_and_destroy(String_Builder *string_builder)
{
    int total_len = 0;
    for (int i = 0; i < string_builder->chunk_count; i++)
    {
        total_len += strlen(string_builder->chunks[i]);
    }

    char *compiled_str = xmalloc(total_len + 1);
    char *compiled_str_ptr = compiled_str;

    for (int i = 0; i < string_builder->chunk_count; i++)
    {
        strcpy(compiled_str_ptr, string_builder->chunks[i]);
        compiled_str_ptr += strlen(string_builder->chunks[i]);
    }

    free(string_builder->chunks);
    string_builder->chunks = NULL;
    string_builder->chunk_count = 0;

    return compiled_str;
}
