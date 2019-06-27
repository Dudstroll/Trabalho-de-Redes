all:
	gcc cliente.c -o cliente -Wall -Wextra -pedantic
	gcc servidor.c -o servidor -lm -Wall -Wextra -pedantic

clean:
	rm *.o
