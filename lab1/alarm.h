#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

void alarmHandler (int signal);
void setAlarmHandler (alarmHandler);