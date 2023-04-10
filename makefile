CC = gcc
FLAGS = -Wall -Wextra -pthread

all: SystemManager sensor user_console

clean: 
	rm -f *.o *~ SystemManager sensor user_console console_pipe sensor_pipe

SystemManager: SystemManager.o SystemManagerFuncs.o log.o
	$(CC) $(FLAGS) -lm $^ -o $@

sensor: sensor.o
	$(CC) $(FLAGS) $^ -o $@

user_console: user_console.o user_console_functions.o
	$(CC) $(FLAGS) $^ -o $@

.c.o:
	$(CC) $(FLAGS) -c $< -o $@

############################################

SystemManager.o: SystemManager.c SystemManager.h
SystemManagerFuncs.o: SystemManagerFuncs.c SystemManager.h
log.o: log.c log.h
sensor.o: sensor.c
user_console.o: user_console.c user_console_functions.h
user_console_functions.o: user_console_functions.c user_console_functions.h
