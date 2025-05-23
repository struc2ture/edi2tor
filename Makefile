.PHONY: run run_standalone clean

bin/platform: src/platform.c bin/editor.dylib | bin
	clang src/platform.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -o bin/platform

bin/editor.dylib: src/editor.c | bin
	clang -dynamiclib src/editor.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -o bin/editor.dylib

bin/editor_standalone: src/editor_standalone.c | bin
	clang src/editor_standalone.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -o bin/editor_standalone

bin:
	mkdir -p bin

run: bin/platform
	bin/platform

run_standalone: bin/editor_standalone
	bin/editor_standalone

clean:
	rm -rf bin
