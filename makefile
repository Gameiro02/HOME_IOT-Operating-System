CC = gcc
FLAGS = -Wall -Wextra -Werror
PROG = main
OBJS = main.o

all: $(PROG)

clean: 
	rm $(OBJS) *~ $(PROG)

$(PROG): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -lm -o $@

.c.o:
	$(CC) $(FLAGS) $< -c -o $@


############################################

main.o: main.c

main: main.o