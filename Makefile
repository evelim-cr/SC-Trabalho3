all: worm

clean:
	@rm -rf worm

rebuild: clean worm

worm: main.c exploit.c recon.c brutexor.c
	gcc -o $@ $^
