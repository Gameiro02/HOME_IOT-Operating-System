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
    char time[9];
    sprintf(time, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Write log entry to the file
    fprintf(log_file, "%s %s\n", time, message);

    // Print the message to the console
    printf("%s %s\n", time, message);
}