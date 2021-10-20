main: main.o ramnet.o
	cc -o rambotc main.o ramnet.o
main.o: main.c ramnet.h
	cc -c main.c
ramnet.o: ramnet.c ramnet.h
	cc -c ramnet.c
clean:
	rm -v rambotc main.o ramnet.o
