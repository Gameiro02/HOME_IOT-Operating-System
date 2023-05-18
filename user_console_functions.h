#ifndef _user_console_functions_h
#define _user_console_functions_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "SystemManager.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ID_LENGTH 32
#define MIN_ID_LENGTH 3
#define CONSOLE_PIPE "console_pipe"

extern pid_t pids[2];

void command_stats(int console_identifier);

void command_reset(int console_identifier);

void command_sensors(int console_identifier);

void command_add_alert(int console_identifier, char *id, char *key, char *min, char *max);

void command_remove_alert(int console_identifier, char *id);

void command_list_alerts(int console_identifier);

void command_exit(int console_identifier);

void read_command(int console_identifier);

void write_to_named_pipe(const char *message, const char *pipe_name);

#endif