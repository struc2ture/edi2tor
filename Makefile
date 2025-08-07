CC = clang
CFLAGS = -g -I/opt/homebrew/include -Ithird_party -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL

editor: bin/platform bin/hub bin/editor.dylib

d: bin/hub bin/editor_scene.dylib
	lldb bin/hub -o run -- bin/editor_scene.dylib

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: editor d bin clean

bin/hub: src/hub/hub.c src/hub/hub.h src/hub/hub_internal.h src/hub/scene.c src/hub/scene.h | bin
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@

bin/platform: src/platform.c src/scene_loader.c src/scene_loader.h | bin
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@

bin/editor.dylib: src/editor.c src/editor.h src/util.h src/shaders.h src/unit_tests.h src/unit_tests.c src/actions.h src/actions.c src/input.h src/input.c src/scene_loader.c src/scene_loader.h src/misc.h src/misc.c src/history.h src/history.c src/scratch_runner.h src/scratch_runner.c src/string_builder.h src/string_builder.c src/text_buffer.h src/text_buffer.c bin/live_cube.dylib | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< -o $@

share/e.o: src/editor.c src/editor.h src/util.h src/shaders.h src/unit_tests.h src/unit_tests.c src/actions.h src/actions.c src/input.h src/input.c src/scene_loader.c src/scene_loader.h src/misc.h src/misc.c src/history.h src/history.c src/scratch_runner.h src/scratch_runner.c | bin
	$(CC) -c $(CFLAGS) $< -o $@

bin/editor_api.o: src/editor.c src/editor.h src/util.h src/shaders.h src/unit_tests.h src/unit_tests.c src/actions.h src/actions.c src/input.h src/input.c src/scene_loader.c src/scene_loader.h src/misc.h src/misc.c src/history.h src/history.c src/scratch_runner.h src/scratch_runner.c | bin
	$(CC) -c $(CFLAGS) -DEDITOR_STATIC_LIB $< -o $@

bin/live_cube.dylib: src/scenes/live_cube.c src/scenes/live_cube.h | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< -o $@

bin/editor_scene.dylib: src/scenes/editor_scene.c bin/editor_api.o | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< bin/editor_api.o -o $@
