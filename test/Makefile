CC = gcc
CFLAGS = -Wall -pedantic -g
MAIN = httpd
OBJS = httpd.o
all : $(MAIN)

$(MAIN) : $(OBJS) httpd.h
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS)

sfind.o : httpd.c httpd.h
	$(CC) $(CFLAGS) -c httpd.c

clean :
	rm *.o $(MAIN) core
