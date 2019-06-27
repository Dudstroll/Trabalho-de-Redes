all:
	gcc cliente.c -o cliente
	gcc servidor.c -o servidor -lm

clean:
	rm *.o
