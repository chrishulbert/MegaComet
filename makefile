all: megacomet megamanager megastart

megacomet: megacomet.c config.h
	gcc megacomet.c -o megacomet -std=c99

megamanager: megamanager.c config.h
	gcc megamanager.c -o megamanager -std=c99

megastart: megastart.c config.h
	gcc megastart.c -o megastart -std=c99

