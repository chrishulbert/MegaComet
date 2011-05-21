all: megacomet megamanager megastart

megacomet: megacomet.c config.h
	clang megacomet.c -lev -o megacomet

megamanager: megamanager.c config.h
	clang megamanager.c -lev -o megamanager

megastart: megastart.c config.h
	clang megastart.c -o megastart

