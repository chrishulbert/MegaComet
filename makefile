all: megacomet megamanager

megacomet: megacomet.c
	clang megacomet.c -lev -o megacomet

megamanager: megamanager.c
	clang megamanager.c -lev -o megamanager
