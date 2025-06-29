CC = clang
CFLAGS = -g -I/opt/homebrew/include -isystemthird_party -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL

editor: bin/platform bin/editor.dylib

bin/platform: src/platform.c src/scene_loader.c src/scene_loader.h | bin
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@

bin/editor.dylib: src/editor.c src/editor.h src/util.h src/shaders.h src/unit_tests.h src/unit_tests.c src/actions.h src/actions.c src/input.h src/input.c src/scene_loader.c src/scene_loader.h src/misc.h src/misc.c src/history.h src/history.c bin/live_cube.dylib | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< -o $@

bin/live_cube.dylib: src/live_cube.c src/live_cube.h | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< -o $@

run: bin/platform bin/editor.dylib
	./bin/platform bin/editor.dylib

runcube: bin/platform bin/live_cube.dylib
	./bin/platform bin/live_cube.dylib

bin:
	mkdir -p bin

debug: bin/platform bin/editor.dylib
	lldb -s .lldb

clean:
	rm -rf bin

.PHONY: editor run runcube bin debug clean
