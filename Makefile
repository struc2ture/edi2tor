CC = clang
CFLAGS = -g -I/opt/homebrew/include -Ithird_party -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable
LFLAGS = -L/opt/homebrew/lib -lglfw -framework OpenGL

CIMGUIRPATH = -Wl,-rpath,/Users/struc/dev/other/cimgui/backend_test/example_glfw_opengl3/build

editor: bin/hub bin/editor.dylib bin/debug.dylib bin/cube.dylib

d: bin/hub bin/editor.dylib bin/debug.dylib
	lldb bin/hub -o run -- bin/editor.dylib

dd: bin/hub bin/debug.dylib
	lldb bin/hub -o run

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: editor d dd bin clean

bin/hub: src/hub/hub.c $(wildcard src/hub/*) | bin
	$(CC) $(CFLAGS) $(LFLAGS) $< -o $@ $(CIMGUIRPATH)

bin/editor_api.o: src/editor/editor.c $(wildcard src/editor/*) $(wildcard src/lib/*) | bin
	$(CC) -c $(CFLAGS) $< -o $@

bin/editor.dylib: src/scenes/editor_scene.c bin/editor_api.o | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< bin/editor_api.o -o $@

bin/cube.dylib: src/scenes/cube.c | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) $< -o $@

bin/debug.dylib: src/scenes/debug_scene.c | bin
	$(CC) -dynamiclib $(CFLAGS) $(LFLAGS) -I/Users/struc/dev/other/cimgui -L/Users/struc/dev/other/cimgui/backend_test/example_glfw_opengl3/build -lcimgui $< -o $@
