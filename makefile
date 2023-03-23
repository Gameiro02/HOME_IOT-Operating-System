CC = gcc
FLAGS = -Wall -Wextra -Werror
PROG = main
OBJS = main.o

all: $(PROG)

clean: 
	rm $(OBJS) *~ $(PROG)
	rm console_pipe sensor_pipe

$(PROG): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) -lm -o $@

.c.o:
	$(CC) $(FLAGS) $< -c -o $@


############################################

main.o: main.c

main: main.o