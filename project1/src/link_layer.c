// Link layer protocol implementation

#include "../include/link_layer.h"

#include "../include/macros.h"

#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

// Mudar buff_size para 256 mais tarde...
#define BUF_SIZE 256

int fd;
int timeout, tries;

struct termios oldtio;
struct termios newtio;

#define BAUDRATE B38400

int infoFlag = 0;

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
        else if(b == A_RX) state = A_RCV;
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
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
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

    timeout = connectionParameters.timeout;
    tries = connectionParameters.nRetransmissions;
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
        

        while(alarmCount < tries){
            
            state = START_STATE;
            STOP = FALSE;
            
            
            int bytes = write(fd, SET, 5);
            sleep(1);

            if(alarmEnabled == FALSE){
                alarm(timeout); // 3s para escrever
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
                //printf("Reading: %x\n", b); 

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
    
    //1º criar o BCC para o dataPacket
    //2º fazer byte stuffing
    //3º criar a nova infoFrame com o dataPacket (ja stuffed) la dentro
    //4º enviar a infoFrame e contar o alarme
    //5º factCheck a frame recebida do llread (ver se tem erros ou assim)
    //6º llwrite so termina quando recebe mensagem de sucesso ou quando o limite de tentativas é excedido
    char bcc2 = 0x00;
    for (int i = 0; i < bufSize; i++) {
        bcc2 = bcc2 ^ buf[i];
    }
    unsigned char infoFrame[600] = {0};

    alarmCount = 0;

    infoFrame[0] = FLAG;
    infoFrame[1] = A;
    infoFrame[2] = (infoFlag << 6); // control
    infoFrame[3] = A ^ (infoFlag << 6);

    //byte stuffing
    int index = 4;
    for(int i = 0; i < bufSize; i++) {
        if (buf[i] == 0x7E) {
            infoFrame[index] = 0x7D;
            index++;
            infoFrame[index] = 0x5E;
            index++;
        }
        else if (buf[i] == 0x7D) {
            infoFrame[index] = 0x7D;
            index++;
            infoFrame[index] = 0x5D;
            index++;
        }
        else {
            infoFrame[index] = buf[i];
            index++;
        }
    }

    // Byte stuffing of the bcc2
    if (bcc2 == 0x7E) {
        infoFrame[index] = 0x7D;
        index++;
        infoFrame[index] = 0x5E;
        index++;
    }
    else if (bcc2 == 0x7D) {
        infoFrame[index] = 0x7D;
        index++;
        infoFrame[index] = 0x5D;
        index++;
    }
    else {
        infoFrame[index] = bcc2;
        index++;
    }
    infoFrame[index] = FLAG;
    index++;

    int STOP = FALSE;
    unsigned char rcv[5];

    while(alarmCount < tries){
        if(alarmEnabled == FALSE){
            write(fd, infoFrame, index);
            sleep(1);
            printf("\n");
            for (int j =0; j < index; j++) {
                printf("%02x|", infoFrame[j]);
            }
            printf("\nInfo Frame sent Ns = %d\n", infoFlag);
            alarm(timeout);
            alarmEnabled = TRUE;
        }

        int response = read(fd, rcv, 5);

        if(response > 0){
            if(rcv[2] != (!infoFlag << 7 | 0x05)){
                printf("\nRECEIVED REJ\n");
                alarmEnabled = FALSE;
                continue;
            }
            else if(rcv[3] != (rcv[1]^rcv[2])){
                printf("\nRR NOT CORRECT\n");
                alarmEnabled = FALSE;
                continue;
            }
            else{
                printf("\nRR correctly received");
                break;
            }
        }
    }
    if(alarmCount >= tries){
        printf("TIME-OUT");
        return -1;
    }

    if(infoFlag) infoFlag = 0;
    else infoFlag = 1;


    //se a resposta for RR mudo o infoflag

    return 0;

}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////


int llread(unsigned char *packet)
{   
    unsigned char c;
    unsigned char infoFrame[600];
    int sizeInfoFrame = 0;
    enum statePacket statePac = packet_START;
    
    while (statePac != packet_STOP) {
        int bytes = read(fd, &c, 1);
        
        if(bytes < 0){
            continue;
        }

        //state machine
        switch(statePac){
            case packet_START:
                if (c == FLAG) {
                    statePac = packet_FLAG1;
                    infoFrame[sizeInfoFrame] = FLAG;
                    sizeInfoFrame++;
                }
                break;
            case packet_FLAG1:
                if (c == FLAG){
                    statePac = packet_FLAG1;
                }
                else {
                    statePac = packet_A;
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                break;
            case packet_A:
                if (c == FLAG) {
                    statePac = packet_STOP;
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                else {
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                break;
            default:
                break;
        }
    }
    statePac = START_STATE;

    for (int j = 0; j< sizeInfoFrame; j++) {
        printf("%02x|", infoFrame[j]);
    }
   printf("infoFrame[2]: %x\n", infoFrame[2]);

    unsigned char rFrame[5];

    if(infoFrame[2] != (infoFlag << 6) || (infoFrame[1]^infoFrame[2]) != infoFrame[3]){
        printf("infoFrame[2]: %x\n", infoFrame[2]);
        printf("INFOFLAG: %x\n", (infoFlag << 6));
        printf("infoFrame[1]^infoFrame[2]) : %x\n", infoFrame[1]^infoFrame[2]);
        printf("infoFrame[3]): %x\n", infoFrame[3]);
        printf("\nInfo Frame not received correctly\nSending REJ.\n");
        rFrame[0] = FLAG;
        rFrame[1] = A;
        rFrame[2] = (!infoFlag << 7) | 0x01;
        rFrame[3] = A ^ rFrame[2];
        rFrame[4] = FLAG;
        write(fd, rFrame, 5);

        printf("return on line 505\n");
        return -1;
    }

    //destuffing
    int index = 0;
    for(int i = 0; i < sizeInfoFrame; i++){
        if(infoFrame[i] == 0x7D && infoFrame[i+1]==0x5E){
            packet[index] = FLAG;
            index++;
            i++;
        }

        else if(infoFrame[i] == 0x7D && infoFrame[i+1]==0x5D){
            packet[index] = 0x7D;
            index++;
            i++;
        }

        else {
            packet[index] = infoFrame[i];
            index++;
        }
    }

    int size = 0; //tamanho da secçao de dados
    /*
    if(packet[4]==0x01){
        size = 256*packet[6]+packet[7]+4 +6; //+4 para contar com os bytes de controlo, numero de seq e tamanho
        for(int i=4; i<size-2; i++){
            BCC2 = BCC2 ^ packet[i];
        }
    }
    */


    
    printf("return on line 543\n");
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics, LinkLayer connectionParameters)
{
    //trasmistor - sends DISC, sends UA
    //receiver - receives DISC, sends DISC
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

            int bytes = write(fd, array, 5);
            sleep(1);
            
            if(alarmEnabled == FALSE){
                alarm(connectionParameters.timeout); // 3s para escrever
                alarmEnabled = TRUE;

            }
            if (bytes < 0){
                printf("Emissor: Failed to send DISC\n");
            }
            else{
                printf("----------Emissor: Sent DISC------------\n");
            }
            
            //receber DISC
            printf("Receiving DISC:\n");
            while (state != STOP_STATE)
            {
                int bytesR= read(fd, &b, 1);
                if(bytesR > 0){
                    
                    printf("Reading: %x\n", b); 

                    state = stateMachineDISC(b, state);
                }  
            }
            if (state == STOP_STATE){
                break;
            }
        }
        if (alarmCount >= connectionParameters.nRetransmissions) printf("Didn't receive DISC\n");
        else printf("---------Emissor: Received DISC-----------\n");

        //mandar UA
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
        }

    }     

    if(connectionParameters.role == LlRx){

        printf("Receiving DISC:\n");
        
        enum setState stateR = START_STATE;
        unsigned char a;
        
        while (stateR != STOP_STATE)
        {
            int b_rcv = read(fd, &a, 1);
            if (b_rcv > 0)
            {
                printf("Reading: %x\n", a);

                stateR = stateMachineDISC(a, stateR);

            }

        }
        printf("-Receptor: Received DISC!\n");
    
        
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
                printf("---------Receptor: Sent DISC----------\n");
            }
            
            printf("Receptor: Receiving UA\n");
            while (state != STOP_STATE)
            {
                int bytesR= read(fd, &c, 1);
                if(bytesR > 0){
                    printf("Reading: %x\n", c); 

                    state = stateMachineUA(c, state);
                }
                
            }
            if(state == STOP_STATE){
                break;
            }
        }      
        

    }
    

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 1;
}
