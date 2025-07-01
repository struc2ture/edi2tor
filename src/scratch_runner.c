#include "scratch_runner.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

time_t scratch_runner__get_file_timestamp(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    fprintf(stderr, "[SCRATCH RUNNER] scratch_runner__get_file_timestamp: Failed to get timestamp for file at %s\n", path);
    return 0;
}

bool scratch_runner__copy_file(const char *src, const char *dest)
{
    FILE *in = fopen(src, "rb");
    if (!in)
    {
        fprintf(stderr, "[SCRATCH RUNNER] scratch_runner__copy_file: failed to open file for read at: %s\n", src);
        return false;
    }

    FILE *out = fopen(dest, "wb");
    if (!out)
    {
        fprintf(stderr, "[SCRATCH RUNNER] scratch_runner__copy_file: failed to open file for write at: %s\n", dest);
        fclose(in);
        return false;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        fwrite(buf, 1, n, out);
    }

    fclose(in);
    fclose(out);

    printf("[SCRATCH RUNNER] scratch_runner__copy_file: copied file to %s\n", dest);
    return true;
}

bool scratch_runner__delete_file(const char *path)
{
    if (remove(path) == 0)
    {
        printf("[SCRATCH RUNNER] scratch_runner__delete_file: deleted file at %s\n", path);
        return true;
    }
    else
    {
        fprintf(stderr,"[SCRATCH RUNNER] scratch_runner__delete_file: failed to deleted file at %s\n", path);
        return false;
    }
}

Scratch_Dylib scratch_runner_dylib_open(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot - path;
    time_t now = time(NULL);

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%.*s_%ld.dylib", (int)base_len, path, now);
    if (!scratch_runner__copy_file(path, temp_path))
    {
        fprintf(stderr, "[SCRATCH RUNNER] Failed to copy dylib from %s to %s\n", path, temp_path);
        goto failed_open;
    }


    Scratch_Dylib dylib = {0};
    dylib.handle = dlopen(temp_path, RTLD_NOW);
    if (!dylib.handle)
    {
        fprintf(stderr, "[SCRATCH RUNNER] Failed to open dylib: %s\n", temp_path);
        goto failed_open;
    }

    #define X(NAME) dylib.NAME = dlsym(dylib.handle, #NAME); \
        if (!dylib.NAME) \
        { \
            fprintf(stderr, "[SCRATCH RUNNER] scratch_runner_dylib_open: Failed to load symbol: '%s' in %s\n", #NAME, temp_path); \
            goto failed_open; \
        }

    X(on_run);

    #undef X

    dylib.original_path = strdup(path);
    dylib.copied_path = strdup(temp_path);
    dylib.timestamp = scratch_runner__get_file_timestamp(path);

    printf("[SCRATCH RUNNER] scratch_runner_dylib_open: opened dylib at %s\n", temp_path);

    return dylib;

failed_open:
    if (dylib.handle) dlclose(dylib.handle);
    scratch_runner__delete_file(temp_path);
    return (Scratch_Dylib){0};
}

void scratch_runner_dylib_close(Scratch_Dylib *dylib)
{
    dlclose(dylib->handle);
    scratch_runner__delete_file(dylib->copied_path);

    printf("[SCRATCH RUNNER] scratch_runner_dylib_close: closed dylib at %s\n", dylib->copied_path);

    free(dylib->original_path);
    free(dylib->copied_path);

    *dylib = (Scratch_Dylib){0};
}
