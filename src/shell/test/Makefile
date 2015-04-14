CFLAGS=-O2 -Wall --std=gnu99 -Wextra -pedantic -Wshadow -Wpointer-arith -Wstrict-prototypes -D_FORTIFY_SOURCE=2
LIBS=-lreadline
SOURCES=comp_rl.c

comp_rl : $(SOURCES)
	gcc -o $@ $(CFLAGS) $(SOURCES) $(LIBS)

clean:
	rm comp_rl




