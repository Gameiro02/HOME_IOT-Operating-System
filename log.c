// Gonçalo Neves  - 2020239361
// André Carvalho - 2020237655

#include "log.h"
#include "SystemManager.h"

void clear_log()
{
    FILE *log_file = fopen("log.log", "w");
    fclose(log_file);
}

void write_log(char *message)
{
    // Get the current time hour:minute:second
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char time[20];

    if (message[0] == '\n')
    {
        message++;
        sprintf(time, "\n%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    else
    {
        sprintf(time, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    // Write log entry to the file
    if (fprintf(log_file, "%s %s\n", time, message) < 0)
    {
        perror("Error writing to log file");
        exit(EXIT_FAILURE);
    }
    else
    {
        fflush(log_file);
    }

    // Print the message to the console
    printf("%s %s\n", time, message);
}