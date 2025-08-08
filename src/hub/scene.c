#include "scene.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "hub_internal.h"

time_t scene_get_file_timestamp(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    hub_warning("Failed to get timestamp for file at %s", path);
    return 0;
}

bool scene_copy_file(const char *src, const char *dest)
{
    FILE *in = fopen(src, "rb");
    if (!in)
    {
        hub_warning("Failed to open file for read at: %s", src);
        return false;
    }

    FILE *out = fopen(dest, "wb");
    if (!out)
    {
        hub_warning("Failed to open file for write at: %s", src);
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

    hub_trace("Copied file to %s", dest);
    return true;
}

bool scene_delete_file(const char *path)
{
    if (remove(path) == 0)
    {
        hub_trace("Deleted file at %s", path);
        return true;
    }
    else
    {
        hub_warning("Failed to delete file at %s", path);
        return false;
    }
}

// ----------------------------------------------------

struct Scene scene_open(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t base_len = dot - path;
    time_t now = time(NULL);

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "%.*s_%ld.dylib", (int)base_len, path, now);
    if (!scene_copy_file(path, temp_path))
    {
        hub_warning("Failed to copy dll from %s to %s", path, temp_path);
        goto failed_open;
    }

    struct Scene s = {0};
    s.handle = dlopen(temp_path, RTLD_NOW);
    if (!s.handle)
    {
        hub_warning("Failed to open dll at %s\n", temp_path);
        goto failed_open;
    }

    #define X(NAME) s.NAME = dlsym(s.handle, #NAME); \
        if (!s.NAME) \
        { \
            hub_warning("Failed to load symbol: '%s' in %s", #NAME, temp_path); \
            goto failed_open; \
        }

    X(on_init);
    X(on_reload);
    X(on_frame);
    X(on_platform_event);
    X(on_destroy);

    #undef X

    // TODO: Handle failed alloc
    s.original_path = strdup(path);
    s.copied_path = strdup(temp_path);
    s.timestamp = scene_get_file_timestamp(path);

    hub_trace("Opened dll at %s", temp_path);

    return s;

failed_open:
    if (s.handle) dlclose(s.handle);
    scene_delete_file(temp_path);
    return (struct Scene){0};
}

void scene_close(struct Scene *s)
{
    dlclose(s->handle);
    scene_delete_file(s->copied_path);

    hub_trace("Closed dll at %s", s->copied_path);

    free(s->original_path);
    free(s->copied_path);

    *s = (struct Scene){0};
}

bool scene_hotreload(struct Scene *s)
{
    time_t current_timestamp = scene_get_file_timestamp(s->original_path);
    if (current_timestamp != 0 && current_timestamp != s->timestamp)
    {
        struct Scene old_s = *s;
        *s = scene_open(old_s.original_path);
        if (!s->handle)
        {
            hub_warning("Failed to reload dll at %s", old_s.original_path);
            *s = old_s;
            s->timestamp = current_timestamp;
        }
        else
        {
            hub_trace("Reloaded dll at %s", s->original_path);
            scene_close(&old_s);
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------

struct Scene *scene_map_add(struct Scene_Map *m, struct Scene scene, const char *name)
{
    bool need_realloc = false;
    if (m->cap == 0)
    {
        m->cap = 4;
        need_realloc = true;
    }
    if (m->cap <= m->size)
    {
        m->cap *= 2;
        need_realloc = true;
    }
    if (need_realloc)
    {
        m->scenes = realloc(m->scenes, m->cap * sizeof(m->scenes[0]));
        m->names = realloc(m->names, m->cap * sizeof(m->names[0]));
    }

    m->scenes[m->size] = scene;
    m->names[m->size] = strdup(name);
    m->size++;

    return &m->scenes[m->size - 1];
}

struct Scene *scene_map_add_debug_scene(struct Scene_Map *m, struct Scene scene)
{
    m->debug_scene = scene;
    return &m->debug_scene;
}

struct Scene *scene_map_get_debug_scene(struct Scene_Map *m)
{
    return &m->debug_scene;
}

struct Scene *scene_map_get_by_name(struct Scene_Map *m, const char *name)
{
    int i;
    for (i = 0; i < m->size; i++)
        if (strcmp(m->names[i], name) == 0)
            break;
    return i < m->size ? &m->scenes[i] : NULL;
}

char *scene_map_get_name(struct Scene_Map *m, struct Scene *s)
{
    int i;
    for (i = 0; i < m->size; i++)
        if (&m->scenes[i] == s)
            break;
    return m->names[i];
}

void scene_map_remove(struct Scene_Map *m, struct Scene *s)
{
    if (s == &m->debug_scene)
    {
        m->debug_scene = (struct Scene){0};
    }
    else
    {
        int to_remove;
        for (to_remove = 0; to_remove < m->size; to_remove++)
            if (&m->scenes[to_remove] == s)
                break;
        
        free(m->names[to_remove]);
        
        for (int i = to_remove; i < m->size - 1; i++)
        {
            m->scenes[i] = m->scenes[i + 1];
            m->names[i] = m->names[i + 1];
        }
    }

    m->size--;
}

bool scene_map_swap_up(struct Scene_Map *m, struct Scene *s)
{
    int i = s - m->scenes;
    if (i > 0)
    {
        struct Scene temp = m->scenes[i - 1];
        m->scenes[i - 1] = m->scenes[i];
        m->scenes[i] = temp;
        char *temp_name = m->names[i - 1];
        m->names[i - 1] = m->names[i];
        m->names[i] = temp_name;
        return true;
    }
    return false;
}

bool scene_map_swap_down(struct Scene_Map *m, struct Scene *s)
{
    int i = s - m->scenes;
    if (i < m->size - 1)
    {
        struct Scene temp = m->scenes[i + 1];
        m->scenes[i + 1] = m->scenes[i];
        m->scenes[i] = temp;
        char *temp_name = m->names[i + 1];
        m->names[i + 1] = m->names[i];
        m->names[i] = temp_name;
        return true;
    }
    return false;
}
