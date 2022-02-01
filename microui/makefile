# OS_NAME=`uname -o 2>/dev/null || uname -s`
# if [ $OS_NAME == "Msys" ]; then
#     GLFLAG="-lopengl32"
# elif [ $OS_NAME == "Darwin" ]; then
#     GLFLAG="-framework OpenGL"
# else
#     GLFLAG="-lGL"
# fi
# CFLAGS="-I../src -Wall -std=c11 -pedantic `sdl2-config --libs` $GLFLAG -lm -O3 -g"
# gcc main.c renderer.c ../src/microui.c $CFLAGS

.PHONY: docs
all: renderer.out docs

docs:
	docco microui-header.h microui-source.c -o ../docs


renderer.out: *.c  *.h
	gcc main.c renderer.c ./microui-source.c \
      -I. -Wall -std=c11 -pedantic `sdl2-config --libs` -lGL -lm -O3 -g -o renderer.out
