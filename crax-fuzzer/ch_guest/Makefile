all: tlib.so symargs symstdin symfile

tlib.o: tlib.c s2e.h
	gcc -c $< -fPIC
tlib.so: tlib.o
	gcc -o $@ -shared -fPIC $< -ldl

symargs.o: symargs.c s2e.h
	gcc -c $<
symargs: symargs.o
	gcc -o $@ $<

symstdin.o: symstdin.c s2e.h
	gcc -c $<
symstdin: symstdin.o
	gcc -o $@ $<

symfile.o: symfile.c s2e.h
	gcc -c $<
symfile: symfile.o
	gcc -o $@ $<

clean:
	rm -f *.o *.so symargs symstdin symfile
