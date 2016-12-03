all: tlib.so args stdin

tlib.o: tlib.c
	gcc -c $< -fPIC
tlib.so: tlib.o
	gcc -o $@ -shared -fPIC $< -ldl

args.o: args.c
	gcc -c $<
args: args.o
	gcc -o $@ $<

stdin.o: stdin.c
	gcc -c $<
stdin: stdin.o
	gcc -o $@ $<

clean:
	rm -f *.o *.so stdin args
