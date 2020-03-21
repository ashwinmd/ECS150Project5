# Generate executable
pfsim: pfsim.o
	gcc -Wall -Wextra -Werror -g -o pfsim pfsim.o

# Generate objects files from C files
pfsim.o: pfsim.c
	gcc -Wall -Wextra -Werror -g -c -o pfsim.o pfsim.c

# Clean generated files
clean:
	rm -f pfsim pfsim.o
