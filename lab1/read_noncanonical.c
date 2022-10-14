// Read from serial port in non-canonical mode
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

#include "macros.h"


// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5 //256
#define TRIES 4

volatile int STOP = FALSE;

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

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

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
    
//------- APAGAR ISTO, INSERIR A STATE MACHINE

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

    //lê os caracteres (um a um) da porta série, em modo não canónico, até receber o caracter de fim de string (‘\0’);
    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        //buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

        printf(":%s:%d\n", buf, bytes);
        if (buf[bytes] == '\0')
            STOP = TRUE;
    }

// ------------------ STATE MACHINE

 // Depois da state machine temos de dar print à string recebida e criar um novo buffer para mandá-la de volta (buf2)
// escrevemos no buffer a string recebida e enviamos. a partir daqui o write_non canonical faz coisas.

    int num_tries = 0;

    while(num_tries < TRIES){

        unsigned char received[BUF_SIZE] = {0}; // string to store message received
        enum setState state = START_STATE;
        int i=0;
        char b;
        
        while (STOP == FALSE){

            int b_rcv= read(fd, &b,1);

            if(b_rcv == 0){
                break;
            }

            received[i] = b;
            i++;        
            printf("Read: %c\0", b);

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
                    if(b == C_SET) state = C_RCV;
                    else if (b == FLAG) state = FLAG_RCV;
                    else state = START_STATE;

                    break;

                case C_RCV:

                    // Para o read em vez de bcc_ua é bcc_set
                    if(b == BCC_SET) state = BCC;
                    else if (b == FLAG) state = FLAG_RCV;
                    else state = START_STATE;

                    break;

                case BCC:

                    if (b == FLAG) state = STOP_STATE;
                    else state = START_STATE;

                    break;

                case STOP_STATE:

                    STOP = TRUE;

                    break;
                
                default:
                    break;
            }
        }

        printf("Received: %s\n", received);

        unsigned char buf[BUF_SIZE] = {0};
        
        // caso a dizer que vai mandar bem. 

        buf[0] = FLAG;
        buf[1] = A;
        buf[2] = C_UA;
        buf[3] = BCC_UA;
        buf[4] = FLAG;
        

        int b_send = write(fd, buf, BUF_SIZE);

        printf("%d bytes written\n", b_send);
        if (b_send < 0) printf("Erro %d", 3);

        num_tries++;
        STOP = FALSE;

        sleep(1);
    }


    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide
    
    // If I want the reader to send a sentence to the writer
    //int bytes = write(fd, buf, strlen(buf)+1);
    //printf("%d bytes written\n", bytes);
    
   
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
