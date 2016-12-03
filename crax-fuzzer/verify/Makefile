all:	symarg symfile symfile2 symstdin tlib.so
tlib.o:	tlib.c
	gcc -c tlib.c
tlib.so:	tlib.o
	gcc -o tlib.so -shared -fPIC tlib.o -ldl
symarg.o:	symarg.c
	gcc -c symarg.c
symarg:		symarg.o
	gcc -o $@ $@.o
symfile.o:	symfile.c
	gcc -c symfile.c
symfile:		symfile.o
	gcc -o $@ $@.o
symfile2.o:	symfile2.c
	gcc -c symfile2.c
symfile2:		symfile2.o
	gcc -o $@ $@.o
symstdin.o:	symstdin.c
	gcc -c symstdin.c
symstdin:		symstdin.o
	gcc -o $@ $@.o
clean:
	rm -f *.o *.so symfile symfile2 symarg symstdin
