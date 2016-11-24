all: worm

clean:
	@rm -rf worm

rebuild: clean worm

worm: main.c exploit.c
	gcc -m32 -static-libgcc -Os -o $@ $^
