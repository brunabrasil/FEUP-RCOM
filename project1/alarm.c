// Alarm example
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    STOP = TRUE;

    printf("Alarm #%d\n", alarmCount);
}

void setAlarmHandler(){
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < 4)
    {
        if (alarmEnabled == FALSE)
        {

            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            //chamar state machine
        }
    }

    printf("Ending program\n");
}
