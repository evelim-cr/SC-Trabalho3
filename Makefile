all: worm

clean:
	@rm -rf worm

rebuild: clean worm

worm: main.c
	gcc -o $@ $^
