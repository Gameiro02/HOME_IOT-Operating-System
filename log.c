#include "log.h"

void write_log(char *message)
{
    // Get the current time hour:minute:second
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char time[9];
    sprintf(time, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

    // Open the log file
    FILE *log_file = fopen("log.txt", "a");
    fprintf(log_file, "%s %s\n", time, message);
    fclose(log_file);

    // Print the message to the console
    printf("%s %s\n", time, message);
}