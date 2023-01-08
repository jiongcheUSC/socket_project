CC = gcc
CFLAGS = -g -Wall
TARGETS = serverM serverA serverB serverC clientA clientB

all:$(TARGETS)

serverM:serverM.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

serverA:serverA.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

serverB:serverB.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

serverC:serverC.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

clientA:clientA.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

clientB:clientB.o common.o
	$(CC) $(CFLAGS) -o $@ $< common.o

common.o:common.c common.h

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@


.PHONY : clean
clean:
	rm *.o
	rm $(TARGETS)