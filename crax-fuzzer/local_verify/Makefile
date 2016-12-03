all: tlib.so stdin

tlib.o: tlib.c
	gcc -c $< -fPIC
tlib.so: tlib.o
	gcc -o $@ -shared -fPIC $< -ldl

stdin.o: stdin.c
	gcc -c $<
stdin: stdin.o
	gcc -o $@ $<

clean:
	rm -f *.o *.so stdin
