// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "macros.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

// Mudar buff_size para 256 mais tarde...
#define BUF_SIZE 5

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    // STOP = TRUE;

    printf("Alarm #%d\n", alarmCount);
}


int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");


// ------------- CRIAR A TRAMA COM AS FLAGS CORRETAS ----

    // Create frame to send
    unsigned char buf[BUF_SIZE] = {0};

    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C_SET;
    buf[3] = BCC_SET;
    buf[4] = FLAG;

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    //buf[5] = '\n';

    int bytes = 0;
    enum setState state;

    // Wait until all bytes have been written to the serial port
    // sleep(1);

    (void)signal(SIGALRM, alarmHandler); // set ao alarme

    unsigned char received[BUF_SIZE] = {0}; // string to store message received
    int i=0;
    char b; 

    while(alarmCount < 4){

        state = START_STATE;
        STOP = FALSE;

        bytes = write(fd, buf, BUF_SIZE);
        printf("%d bytes written\n", bytes);
        /*if (bytes < 0){
            printf("Erro %s", strerror(errno));
            */

        alarm(3); // 3s para escrever
        
        while (STOP == FALSE){

            int b_rcv= read(fd, &b,1);

            if(b_rcv == 0){
                break;
            }
                
            received[i] = b;
            i++;        
            //printf("Read in emissor: %c\n", b);

            switch (state) {

                case START_STATE:

                    // se encontrar FLAG_RCV passa pra o proximo state
                    if(b == FLAG) state = FLAG_RCV;
                    break;

                case FLAG_RCV:

                    // se encontrar A_RCV parra pro proximo state
                    if(b == A) state = A_RCV;

                    // se encontrar a mesma flag, FLAG_RCV, fica no mesmo estado
                    else if (b == FLAG) state = FLAG_RCV;

                    // se encontrar qualquer outra flag, volta para o estado START_STATE
                    else state = START_STATE;

                    break;

                case A_RCV:

                    // Para o read em vez de c_ua é c_set
                    if(b == C_UA) state = C_RCV;
                    else if (b == FLAG) state = FLAG_RCV;
                    else state = START_STATE;

                    break;

                case C_RCV:

                    // Para o read em vez de bcc_ua é bcc_set
                    if(b == BCC_UA) state = BCC;
                    else if (b == FLAG) state = FLAG_RCV;
                    else state = START_STATE;
//wsl 
                    break;

                case BCC:

                    if (b == FLAG){
                        state = STOP_STATE;
                        STOP = TRUE;
                    }
                    else state = START_STATE;

                    break;

                case STOP_STATE:

                    STOP = TRUE;

                    break;
                
                default:
                    break;
            }
        }

        i = 0;

        if (state == STOP_STATE){
            break;
        }
    }
    
    // teve mais tentativas do que suposto
    if(alarmCount == 4){
        printf("ERRO NO UA\n");
    }

    printf("Ending\n");

    // dar print à string recebida
    int j = 0;
    while (j < BUF_SIZE){
        printf("%u ", received[j]);
        j++;
    }
   
    // CHAMAR O HANDLER PARA COMEÇAR A CONTAGEM DE TENTATIVAS
    // EUNQUANTO NÃO ATINGIRMOS A CONTAGEM VAMOS ESCREVER COM AJUDA DA STATE MACHINE AGAIN
    // SE A CONTAGEM ATINGIR 4S, O PROGRAMA TEVE ERRO 
    // SENÃO, IMPRIMIMOS E TA TUDO CERTO

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
