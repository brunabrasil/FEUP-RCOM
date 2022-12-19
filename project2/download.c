#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

typedef struct data {
    char user[128];
    char password[128];
    char host[256];
    char url_path[240];
    char fileName[128];
    char ip[128];   
} data;

// function given by professors
int getIp(char *host, struct data *data){
    struct hostent *h;

    if ((h = gethostbyname(host)) == NULL){
        herror("Error: gethostbyname()");
        return 1;
    }

    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    strcpy(data->ip,inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

int getFileName(struct data *infoData){
  char fullpath[256];

  strcpy(fullpath, infoData->url_path);

  char* token = strtok(fullpath, "/");

  while(token != NULL){
    strcpy(infoData->fileName, token);
    token = strtok(NULL, "/");
  }

  return 0;
}

int parseData(char *url, struct data *infoData ){

    char* ftp = strtok(url, "/");       // ftp:
    char* credentials = strtok(NULL, "/");  // [<user>:<password>@]<host>
    char* url_path = strtok(NULL, "");      // <url-path>

    if (strcmp(ftp, "ftp:") != 0){
        printf("Error: Not using ftp\n");
        return 1;
    }

    char* user = strtok(credentials, ":");
    char* password = strtok(NULL, "@");

    // no user:password given
    if (password == NULL){
        user = "anonymous";
        password = "anonymous";
        strcpy(infoData->host, credentials);
    } else{
        strcpy(infoData->host, strtok(NULL, ""));
    }

    strcpy(infoData->url_path, url_path);
    strcpy(infoData->user, user);
    strcpy(infoData->password, password);

    if(getIp(infoData->host, infoData) != 0){
        printf("Error: resolving host name\n");
        return 1;
    }

    if(getFileName(infoData) != 0){
        perror("Error: reading filename\n");
        exit(1);
    }

    return 0;
}

// function given by professors
int startConnection(char *ip, int port, int *sockfd){
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error: socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(*sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error: connect()");
        exit(-1);
    }

    return 0;
}

void gerIpAndPort(char * ip, int *port, FILE * readSockect){
    char* buf;
    size_t bytesRead;

    while(1){
        getline(&buf, &bytesRead, readSockect);
        printf("> %s", buf);
        if(buf[3] == ' '){
            break;
        }
    }

    strtok(buf, "(");
    char* ip1 = strtok(NULL, ",");
    char* ip2 = strtok(NULL, ",");
    char* ip3 = strtok(NULL, ",");
    char* ip4 = strtok(NULL, ",");
    char* port1 = strtok(NULL, ",");
    char* port2 = strtok(NULL, ")");

    sprintf(ip, "%s.%s.%s.%s", ip1, ip2, ip3, ip4);
    *port = atoi(port1)*256 + atoi(port2);
}

int sendCommand(int sockfd, char * command){
    int bSent;

    bSent = write(sockfd, command, strlen(command));

    if (bSent == 0){
        printf("sendCommand: Connection closed\n");
        return 1;
    } else if (bSent == -1){
        printf("sending command\n");
        return 1;
    }

    printf("%s\n",command);

    return 0;
}

int readReply(FILE * readSockect){
    long code;
    char *aux;
    
    char *buf;
    size_t bufsize = 256;
    size_t characters;

    while(1){
        buf = (char*)malloc(bufsize * sizeof(char));

        characters = getline(&buf,&bufsize,readSockect);
        printf("> %s", buf);
        
        if((characters > 0) && (buf[3] == ' ')){
            code = strtol(buf, &aux, 10); 
            
            if(code >= 500 && code <= 559){
                printf("Error!!\n");
                exit(1);
            }
            break;
        }

        free(buf);
    }  

    return 0;
}

int readFile(char *fileName, int sockfdReceive ){
    FILE *file = fopen(fileName, "w");

    size_t bRead;
    char c[1];

    bRead = read(sockfdReceive, c, 1);

    while(bRead != 0) {
        if(bRead > 0){
            fputc(c[0], file);        
        }
        bRead = read(sockfdReceive, c, 1);
    }
        
    fclose(file);
    return 0;
}


int main(int argc, char **argv) {

    if(argc != 2){
          printf("Incorrect program usage\n"
                "Usage: download ftp://<user>:<password>@<host>/<url> \n");
        exit(1);
    }

    struct data dataInfo;
    int sockfd, sockfdReceive;

    if(parseData(argv[1], &dataInfo) != 0){
        printf("Error: Parsing input\n");
        return 1;
    }
    
    if(startConnection(dataInfo.ip, 21, &sockfd) != 0){
        printf("Error: Starting connection\n");
        return 1;
    }

    FILE * readSockect = fdopen(sockfd, "r");
    readReply(readSockect);
    
    // credentials
    char command[256];
    sprintf(command, "user %s\n", dataInfo.user);

    sendCommand(sockfd,command);
    readReply(readSockect);

    sprintf(command, "pass %s\n",dataInfo.password);

    sendCommand(sockfd,command);
    readReply(readSockect);
    
    // set mode
    sprintf(command, "pasv \n");
    sendCommand(sockfd,command);

    //read ip and port 
    char ip[32];
    int port;
    gerIpAndPort(ip, &port, readSockect);
    printf("ip: %s\n",ip);
    printf("port: %i\n", port);

    //open connection to receive data
    if(startConnection(ip, port, &sockfdReceive) != 0){
        printf("Error: Starting connection\n");
        return 1;
    }

    sprintf(command, "retr %s\r\n",dataInfo.url_path);
    sendCommand(sockfd,command);
    readReply(readSockect);

    readFile(dataInfo.fileName, sockfdReceive);

    //close
    sprintf(command, "quit \r\n");
    sendCommand(sockfd,command);
    
    return 0;
}