bin/editor: src/main.c
	clang src/main.c -lglfw -framework OpenGL -I/opt/homebrew/include -isystemthird_party -L/opt/homebrew/lib -DGL_SILENCE_DEPRECATION -Wall -Wextra -Werror -o bin/editor

run: bin/editor
	bin/editor
