CC     = gcc
LIBS1   = -lreadline
LIBS2   = -pthread

all:
	$(CC) ep1sh.c -o ep1sh $(LIBS1)
	$(CC) ep1.c -o ep1 $(LIBS2)

clean: 
	rm -rf ep2