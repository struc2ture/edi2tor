bin/editor: src/main.c | bin
	clang src/main.c -g -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -o bin/editor

bin:
	mkdir -p bin

run: bin/editor
	bin/editor

clean:
	rm -r bin
