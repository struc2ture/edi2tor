.PHONY: run dl clean debug

bin/platform: src/platform.c bin/editor.dylib | bin
	clang src/platform.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -o bin/platform

bin/editor.dylib: src/editor.c src/editor.h src/util.h src/shaders.h src/unit_tests.c | bin
	clang -dynamiclib src/editor.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -o bin/editor.dylib

bin/live_cube.dylib: src/live_cube.c src/live_cube.h | bin
	clang -dynamiclib src/live_cube.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -o bin/live_cube.dylib

bin:
	mkdir -p bin

run: bin/platform
	bin/platform

debug: bin/platform
	lldb -s .lldb

dl: bin/editor.dylib

clean:
	rm -rf bin
