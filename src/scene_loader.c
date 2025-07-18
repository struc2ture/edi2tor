#include "scene_loader.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

time_t scene_loader__get_file_timestamp(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    fprintf(stderr, "[SCENE LOADER] scene_loader__get_file_timestamp: Failed to get timestamp for file at %s\n", path);
    return 0;
}

bool scene_loader__copy_file(const char *src, const char *dest)
{
    FILE *in = fopen(src, "rb");
    if (!in)
    {
        fprintf(stderr, "[SCENE LOADER] scene_loader__copy_file: failed to open file for read at: %s\n", src);
        return false;
    }

    FILE *out = fopen(dest, "wb");
    if (!out)
    {
        fprintf(stderr, "[SCENE LOADER] scene_loader__copy_file: failed to open file for write at: %s\n", dest);
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

    printf("[SCENE LOADER] scene_loader__copy_file: copied file to %s\n", dest);
    return true;
}

bool scene_loader__delete_file(const char *path)
{
    if (remove(path) == 0)
    {
        printf("[SCENE LOADER] scene_loader__delete_file: deleted file at %s\n", path);
        return true;
    }
    else
    {
        fprintf(stderr,"[SCENE LOADER] scene_loader__delete_file: failed to deleted file at %s\n", path);
        return false;
    }
}

Scene_Dylib scene_loader_dylib_open(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot - path;
    time_t now = time(NULL);

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%.*s_%ld.dylib", (int)base_len, path, now);
    if (!scene_loader__copy_file(path, temp_path))
    {
        fprintf(stderr, "[SCENE LOADER] Failed to copy dylib from %s to %s\n", path, temp_path);
        goto failed_open;
    }


    Scene_Dylib dylib = {0};
    dylib.handle = dlopen(temp_path, RTLD_NOW);
    if (!dylib.handle)
    {
        fprintf(stderr, "[SCENE LOADER] Failed to open dylib: %s\n", temp_path);
        goto failed_open;
    }

    #define X(NAME) dylib.NAME = dlsym(dylib.handle, #NAME); \
        if (!dylib.NAME) \
        { \
            fprintf(stderr, "[SCENE LOADER] scene_loader_dylib_open: Failed to load symbol: '%s' in %s\n", #NAME, temp_path); \
            goto failed_open; \
        }

    X(on_init);
    X(on_reload);
    X(on_frame);
    X(on_platform_event);
    X(on_destroy);

    #undef X

    dylib.original_path = strdup(path);
    dylib.copied_path = strdup(temp_path);
    dylib.timestamp = scene_loader__get_file_timestamp(path);

    printf("[SCENE LOADER] scene_loader_dylib_open: opened dylib at %s\n", temp_path);

    return dylib;

failed_open:
    if (dylib.handle) dlclose(dylib.handle);
    scene_loader__delete_file(temp_path);
    return (Scene_Dylib){0};
}

void scene_loader_dylib_close(Scene_Dylib *scene_dylib)
{
    dlclose(scene_dylib->handle);
    scene_loader__delete_file(scene_dylib->copied_path);

    printf("[SCENE LOADER] scene_loader_dylib_close: closed dylib at %s\n", scene_dylib->copied_path);

    free(scene_dylib->original_path);
    free(scene_dylib->copied_path);

    *scene_dylib = (Scene_Dylib){0};
}

bool scene_loader_dylib_check_and_hotreload(Scene_Dylib *scene_dylib)
{
    time_t current_timestamp = scene_loader__get_file_timestamp(scene_dylib->original_path);
    if (current_timestamp != 0 && current_timestamp != scene_dylib->timestamp)
    {
        Scene_Dylib old_dylib = *scene_dylib;
        *scene_dylib = scene_loader_dylib_open(old_dylib.original_path);
        if (!scene_dylib->handle)
        {
            fprintf(stderr, "[SCENE LOADER] Failed to reload dylib at %s.\n", old_dylib.original_path);
            *scene_dylib = old_dylib;
            scene_dylib->timestamp = current_timestamp;
        }
        else
        {
            printf("[SCENE LOADER] Reloaded dylib at %s.\n", scene_dylib->original_path);
            scene_loader_dylib_close(&old_dylib);
            return true;
        }
    }
    return false;
}
