// Link layer protocol implementation

#include "../include/link_layer.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../include/macros.h"

#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

// Mudar buff_size para 256 mais tarde...
#define BUF_SIZE 256

int fd;

struct termios oldtio;
struct termios newtio;

#define BAUDRATE B38400

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    //STOP = TRUE;

    printf("Alarm #%d\n", alarmCount);
}

enum setState stateMachineUA (unsigned char b, enum setState state){

    switch (state)
    {
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
        break;

    case BCC:
        if (b == FLAG){
            state = STOP_STATE;
            STOP = TRUE;
            }
        break;

    case STOP_STATE:
        STOP = TRUE;
        break;

    default:
        break;

    }
    return state;
}

enum setState stateMachineSET(unsigned char b, enum setState state){
    switch (state)
    {
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
        if(b == BCC_SET){
            state = BCC;

        } 
        else if (b == FLAG) state = FLAG_RCV;
        else state = START_STATE;
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

    return state;
}

enum setState stateMachineDISC(unsigned char b, enum setState state){
    switch (state)
    {
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
        if(b == C_DISC) state = C_RCV;
        else if (b == FLAG) state = FLAG_RCV;
        else state = START_STATE;
        break;
    case C_RCV:
        // Para o read em vez de bcc_ua é bcc_set
        if(b == A^C_DISC){
            state = BCC;
        } 
        else if (b == FLAG) state = FLAG_RCV;
        else state = START_STATE;
        break;
    case BCC:
        if (b == FLAG){
            state = STOP_STATE;
            STOP = TRUE;
        }
        else state = START_STATE;
        break;
    default:
        break;
    }

    return state;
}



// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)

{
    (void)signal(SIGALRM, alarmHandler);
    // printf("%s", connectionParameters.serialPort);
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror(connectionParameters.serialPort);
        return -1;
    }

    struct termios oldtio;
    struct termios newtio;
    
    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        return -1;
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
    tcflush(fd, TCIOFLUSH);
    
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    printf("ROLE: %d\n", connectionParameters.role);

    if (connectionParameters.role == LlTx){
        alarmCount = 0;
        printf("I am the Emissor\n");

        printf("New termios structure set\n");
        enum setState state;
        unsigned char b;
        unsigned char SET[5];
        SET[0] = FLAG;
        SET[1] = A;
        SET[2] = C_SET;
        SET[3] = BCC_SET;
        SET[4] = FLAG;
        

        while(alarmCount < connectionParameters.nRetransmissions){
            
            state = START_STATE;
            STOP = FALSE;
            
            
            int bytes = write(fd, SET, 5);
            sleep(1);

            if(alarmEnabled == FALSE){
                alarm(connectionParameters.timeout); // 3s para escrever
                alarmEnabled = TRUE;
            }


            if (bytes < 0){
                printf("Failed to send SET\n");
            }
            else{
                printf("Sending: %x,%x,%x,%x,%x\n", SET[0], SET[1], SET[2], SET[3], SET[4]);

                printf("Sent SET FRAME\n");
            }

            
            while (STOP == FALSE)
            {
                int b_rcv= read(fd, &b, 1);
                if(b_rcv == 0){
                    continue;
                }
                printf("Reading: %x\n", b); 

                state = stateMachineUA(b, state);
                
            }
            
            
           if (state == STOP_STATE){
                break;
            }
                  
        }

        if (alarmCount == connectionParameters.nRetransmissions) printf("Error UA\n");
        else printf("Received UA successfully\n");
    }

    else if (connectionParameters.role == LlRx)
    {
        printf("I am the Receptor\n");

        enum setState stateR = START_STATE;
        unsigned char c;

        while (stateR != STOP_STATE)
        {
            int b_rcv = read(fd, &c, 1);
            if (b_rcv > 0)
            {
                printf("Receiving: %x\n", c);

                stateR = stateMachineSET(c, stateR);
            }

        }

        printf("Received SET FRAME\n");

        unsigned char UA[5];
        UA[0] = FLAG;
        UA[1] = A;
        UA[2] = C_UA;
        UA[3] = BCC_UA;
        UA[4] = FLAG;
        int bytesReceptor = write(fd, UA, 5);
        if (bytesReceptor < 0){
            printf("Failed to send UA\n");
        }
        else {
            printf("Sending: %x,%x,%x,%x,%x\n", UA[0], UA[1], UA[2], UA[3], UA[4]);
            printf("Sent UA FRAME\n");
        }

    }
    else {
        printf("ERROR: Unknown role!\n");
        exit(1);
    }


    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics, LinkLayer connectionParameters)
{
    // TODO
    //trasmistor - sends DISC, receives UA
    //receiver - receives DISC in llread? , sends UA
    printf("------------------------LLCLOSE--------------------\n");
    signal(SIGALRM,alarmHandler);
    printf("ROLE: %d\n", connectionParameters.role);

    if(connectionParameters.role == LlTx){
        alarmCount = 0;
        unsigned char array[5];
        array[0] = FLAG;
        array[1] = A;
        array[2] = C_DISC;
        array[3] = A^C_DISC;
        array[4] = FLAG;

        while(alarmCount < connectionParameters.nRetransmissions){

            enum setState state = START_STATE;
            unsigned char b;

            
            if(alarmEnabled == FALSE){
                int bytes = write(fd, array, 5);

                
                alarm(connectionParameters.timeout); // 3s para escrever
                alarmEnabled = TRUE;
                if (bytes < 0){
                printf("Emissor: Failed to send DISC\n");
                }
                else{
                    printf("Emissor: Sent DISC\n");
                }
            }
            /*
            while (state != STOP_STATE)
            {
                int bytesR= read(fd, &b, 1);
                printf("Reading: %d\n", bytesR); 
                if(bytesR > 0){
                    printf("alarmCount #%d\n", alarmCount);  
                    printf("Reading: %x\n", b); 

                    state = stateMachineDISC(b, state);
                    printf("\nESTADOOOO %d\n", state);

                }
                
                
            }
            printf("Emissor: Received DISC\n");

            unsigned char UA[5];
            UA[0] = FLAG;
            UA[1] = A;
            UA[2] = C_UA;
            UA[3] = BCC_UA;
            UA[4] = FLAG;
            int bytesUA = write(fd, UA, 5);
            if (bytesUA < 0){
                printf("Emissor: Failed to send UA\n");
            }
            else {
                printf("Emissor: Sending UA: %x,%x,%x,%x,%x\n", UA[0], UA[1], UA[2], UA[3], UA[4]);

                printf("Sent UA FRAME\n");
            }*/
           

        }

    }     

    if(connectionParameters.role == LlRx){
        printf("\n ESTADOOokkkk");
        
        enum setState stateR = START_STATE;
        printf("\n ESTADOO:");
        unsigned char a;
        
        
        while (stateR != STOP_STATE)
        {
            int b_rcv = read(fd, &a, 1);
            printf("Receiving: %d\n", b_rcv);
            if (b_rcv > 0)
            {
                printf("Receiving: %x\n", a);

                stateR = stateMachineDISC(a, stateR);
                printf("\n ESTADO %d \n", stateR);

            }

        }
        printf("Receptor received DISC!");
        
        

        /*
        unsigned char array[5];
        array[0] = FLAG;
        array[1] = A_RX;
        array[2] = C_DISC;
        array[3] = A_RX^C_DISC;
        array[4] = FLAG;
        alarmCount = 0;
        
        while(alarmCount < connectionParameters.nRetransmissions){

            enum setState state = START_STATE;
            unsigned char c;

            int bytes = write(fd, array, 5);
            sleep(1);
            if(alarmEnabled == FALSE){
                alarm(connectionParameters.timeout); // 3s para escrever
                alarmEnabled = TRUE;
            }
            if (bytes < 0){
                printf("Receptor: Failed to send DISC\n");
            }
            else{
                printf("Sent DISC\n");
            }
            while (state != STOP_STATE)
            {
                int bytesR= read(fd, &c, 1);
                if(bytesR == 0){
                    continue;
                }
                printf("alarmCount #%d\n", alarmCount);  
                printf("Reading: %x\n", c); 

                state = stateMachineUA(c, state);
                
            }
        }        
        */

        

    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);



    return 1;
}
