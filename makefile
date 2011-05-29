all: megacomet megamanager megastart

flags = -std=c99 -D_GNU_SOURCE -lev

megacomet: megacomet.c config.h
	gcc megacomet.c -o megacomet $(flags)

megamanager: megamanager.c config.h
	gcc megamanager.c -o megamanager $(flags)

megastart: megastart.c config.h
	gcc megastart.c -o megastart $(flags)
