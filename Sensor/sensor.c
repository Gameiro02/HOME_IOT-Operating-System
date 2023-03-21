#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../UserConsole/user_console_functions.h"
#include <signal.h>

// Function to the process senssor

#define SENSOR_PIPE "./sensor_pipe"

struct sensor
{
    char id[MAX_ID_LENGTH];
    int interval;
    char key[MAX_ID_LENGTH];
    int min;
    int max;
    int num_messages;
};

struct sensor s1;

void handle_sigint(int sig)
{
    printf("Received SIGINT signal. Exiting...\n");
    exit(0);
}

void handle_sigtstp(int sig)
{
    printf("Number of messages sent: %d\n", s1.num_messages);
}

char *sensor_to_string(struct sensor *s)
{
    char *str = malloc(sizeof(char) * 100);                            // Alocar espaço suficiente para a string resultante
    sprintf(str, "ID %s#%s#%d", s->id, s->key, (s->min + s->max) / 2); // Formatar a string com as informações do sensor
    return str;
}

int main(int argc, char *argv[])
{
    s1.num_messages = 0;

    if (argc != 6)
    {
        printf("Wrong number of arguments\n");
        return 1;
    }

    if (strlen(argv[1]) < MIN_ID_LENGTH || strlen(argv[1]) > MAX_ID_LENGTH)
    {
        printf("Id must have between %d and %d characters\n", MIN_ID_LENGTH, MAX_ID_LENGTH);
        return 1;
    }

    strcpy(s1.id, argv[1]);

    s1.interval = atoi(argv[2]);

    if (s1.interval < 0)
    {
        printf("Interval must be greater than or equal to 0\n");
        return 1;
    }

    if (strlen(argv[3]) < MIN_ID_LENGTH || strlen(argv[3]) > MAX_ID_LENGTH)
    {
        printf("Key must have between %d and %d characters\n", MIN_ID_LENGTH, MAX_ID_LENGTH);
        return 1;
    }

    strcpy(s1.key, argv[3]);

    s1.min = atoi(argv[4]);
    s1.max = atoi(argv[5]);

    if (s1.min > s1.max)
    {
        printf("Min must be less than or equal to max\n");
        return 1;
    }

    // Signal handlers
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);

    // Create the String to Send
    char *str = sensor_to_string(&s1);

    printf("%s\n", str);

    // Send to the pipe
    // FILE *pipe = fopen(SENSOR_PIPE, "w");

    // if (pipe == NULL)
    // {
    //     printf("Error opening pipe\n");
    //     return 1;
    // }

    // fprintf(pipe, "%s", str);

    // fclose(pipe);
}

// Run: gcc sensor.c -o sensor; ./sensor SENS1 3 HOUSETEMP 10 100