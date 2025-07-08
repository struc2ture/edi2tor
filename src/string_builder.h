#pragma once

typedef struct {
    char **chunks;
    int chunk_count;
} String_Builder;


void string_builder_append_f(String_Builder *string_builder, const char *fmt, ...);
void string_builder_append_str_range(String_Builder *string_builder, const char *str, int start, int count);
char *string_builder_compile_and_destroy(String_Builder *string_builder);
