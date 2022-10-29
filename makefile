main: main.o ramnet.o
	cc -Os -Wall -o rambotc main.o ramnet.o
main.o: main.c ramnet.h
	cc -Os -Wall -c main.c
ramnet.o: ramnet.c ramnet.h
	cc -Os -Wall -c ramnet.c
clean:
	rm -v rambotc main.o ramnet.o
