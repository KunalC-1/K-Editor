main :  main.o piecetable.o
		gcc -o main main.o piecetable.o
main.o : main.c piecetable.h
		gcc -c main.c

piecetable.o : piecetable.c 
		gcc -c piecetable.c

clean :
		rm -f main *.o
