#pragma once

#include <stdbool.h>
#include <time.h>

typedef void (*scratch_on_run_t)(void *state);

typedef struct {
    void *handle;
    char *original_path;
    char *copied_path;
    time_t timestamp;
    scratch_on_run_t on_run;
} Scratch_Dylib;

time_t scratch_runner__get_file_timestamp(const char *path);
bool scratch_runner__copy_file(const char *src, const char *dest);
bool scratch_runner__delete_file(const char *path);
Scratch_Dylib scratch_runner_dylib_open(const char *path);
void scratch_runner_dylib_close(Scratch_Dylib *dylib);
