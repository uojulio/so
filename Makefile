all: balcao medico cliente

balcao: MEDICALso.o balcao.o common.o
	gcc MEDICALso.o balcao.o common.o -pthread -o balcao

medico: medico.o common.o
	gcc medico.o common.o -o medico

cliente: cliente.o common.o
	gcc cliente.o common.o -o cliente

MEDICALso.o: ./src/MEDICALso.c
	gcc -c ./src/MEDICALso.c

balcao.o: ./src/balcao.c ./src/balcao.h
	gcc -c ./src/balcao.c

common.o: ./src/common.c ./src/common.h
	gcc -c ./src/common.c

medico.o: ./src/medico.c
	gcc -c ./src/medico.c

cliente.o: ./src/cliente.c
	gcc -c ./src/cliente.c

clean:
	rm *.o balcao medico cliente