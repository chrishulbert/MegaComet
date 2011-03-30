all: megacomet
megacomet: megacomet.c
	clang megacomet.c -lev -o megacomet
