// Application layer protocol implementation

#include "../include/application_layer.h"

int createControlPacket(char* filename, int start, unsigned char* packet){
    
    //L1 é so um byte ent V (o filename) n pode passar de 255
    if(strlen(filename) > 255) {
        printf("size of filename can't fit in 1 byte\n");
        return -1;
    }
    struct stat st;
    stat(filename, &st);
    unsigned char size_string[20]; //pq 20? ou 10
    int fileSize = st.st_size;
    sprintf(size_string, "%02lx", fileSize); // stores the size of the file in a string , 02lX
    int sizeBytes = strlen(size_string) / 2;

    if(start) packet[0] = 0x02; //start
    else packet[0] = 0x03; // end

    packet[1] = 0; // 0 -> tamanho do ficheiro 
    packet[2] = sizeBytes;

    int i = 4;
    for(int j = (sizeBytes - 1); j>-1; j--){
		packet[i] = fileSize >> (j*8);
        i++;
	}

    packet[i] = 1; // 1 -> name of the file
    i++; 

    int filename_size = strlen(filename);
    
    packet[i] = filename_size;
    i++;
    for(int j = 0; j < filename_size; j++){
		packet[i] = filename[i];
        i++;
	}

    return i;

}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
    printf("in the application layer\n");

    LinkLayer ll;
    strcpy(ll.serialPort, serialPort);
    ll.baudRate = baudRate;
    if(strcmp(role, "rx") == 0){
        ll.role = LlRx;
    }
    else if(strcmp(role,"tx") == 0){
        ll.role = LlTx;
    }
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;

    if (llopen(ll) == -1) {
        printf("\nERROR: Couldn't estabilish the connection\n");
        llclose(0, ll);
        return;
    } else {
        printf("\nConnection estabilished\n");
    }

    //start reading penguin
    unsigned char packet[300];

    if(ll.role == LlTx){
        // 1. Create controlPacket
        // 2. Call the llwrite to create the info frame
        // 2. Make byte stuffing on the array
        // 3. Send the information frame to the receiver
        //pq 300?
        unsigned char bytes[200];
        
        //cotinuar dps
        int nBytes = 200;
        FILE *file = fopen(filename, "rb");

        if(!file){
            printf("Could not open file \n");
            return;
        }
        else {
            printf("Open file successfully\n");
        }

        fclose(file);
        int sizePac = createControlPacket(filename, 1, &packet);

        llwrite(packet, sizePac);
        //se for -1, lidar com essa informação, n conseguiu mandar

    }
    else if (ll.role == LlRx){
        while(TRUE){
            int ret = llread(packet);
            //printf("PACKET 0: %02x \n", packet[0]);

            if (ret == -1) {
                continue;
            }
            if(ret == 0) {
                break;
            }
            if(packet[0] = 0x03){
                //break;
            }
        }
        
    }

    llclose(1, ll);
}