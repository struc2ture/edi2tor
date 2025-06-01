.PHONY: run dl clean debug

bin/platform: src/platform.c bin/editor.dylib | bin
	clang src/platform.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -o bin/platform

bin/editor.dylib: src/editor.c src/editor.h src/util.h | bin
	clang -dynamiclib src/editor.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -Wno-unused-function -o bin/editor.dylib

bin:
	mkdir -p bin

run: bin/platform
	bin/platform

debug: bin/platform
	lldb -s .lldb

dl: bin/editor.dylib

clean:
	rm -rf bin
