#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

#define TRUE 1

typedef struct urlData {
    char user[128];
    char password[128];
    char host[256];
    char url_path[240];
    char fileName[128];
    char ip[128];   
} urlData;

// function given by professors
int getIp(char *host, struct urlData *urlData){
    struct hostent *h;

    if ((h = gethostbyname(host)) == NULL){
        herror("Error in gethostbyname()");
        return -1;
    }

    strcpy(urlData->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

int getFileName(struct urlData *urlData){
  char fullpath[256];

  strcpy(fullpath, urlData->url_path);

  char* token = strtok(fullpath, "/");

  while(token != NULL){
    strcpy(urlData->fileName, token);
    token = strtok(NULL, "/");
  }

  return 0;
}

int parseUrlData(char *url, struct urlData *urlData ){

    char* ftp = strtok(url, "/");
    char* credentials = strtok(NULL, "/");  // [<user>:<password>@]<host>
    char* url_path = strtok(NULL, "");      // url-path

    if (strcmp(ftp, "ftp:") != 0){
        printf("Error: Not using ftp protocol\n");
        return -1;
    }

    char* user = strtok(credentials, ":");
    char* password = strtok(NULL, "@");

    // no user:password given
    if (password == NULL){
        user = "anonymous";
        password = "anonymous";
        strcpy(urlData->host, credentials);
    } else {
        strcpy(urlData->host, strtok(NULL, ""));
    }

    strcpy(urlData->url_path, url_path);
    strcpy(urlData->user, user);
    strcpy(urlData->password, password);

    if(getIp(urlData->host, urlData) != 0){
        printf("Error: resolving host name\n");
        return 1;
    }

    if(getFileName(urlData) != 0){
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
    char *line = NULL;
    size_t len = 0;

    while(TRUE){
        getline(&line, &len, readSockect);
        printf("> %s", line);
        if(line[3] == ' '){
            break;
        }
    }

    int ip_address[4];
    int port_address[2];

    sscanf(line, "227 Entering Passive Mode (%d, %d, %d, %d, %d, %d).\n", &ip_address[0], &ip_address[1], &ip_address[2], &ip_address[3], &port_address[0], &port_address[1]);
    free(line);

    sprintf(ip, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    *port = port_address[0] * 256 + port_address[1];
}
 
int sendCommandToServer (int sockfd, char * command){
    int bSent;

    bSent = write(sockfd, command, strlen(command));

    if (bSent == 0){
        printf("sendCommand: Connection closed\n");
        return 1;
    } else if (bSent == -1){
        printf("Error: sending command\n");
        return 1;
    }

    printf("%s\n",command);

    return 0;
}

int readServerReply(FILE * readSockect){
    long code;
    char *buf;
    size_t bufsize = 0;

    while(getline(&buf, &bufsize, readSockect) != -1){
        
        printf("> %s", buf);
        
        if(buf[3] == ' '){
            code = atoi(buf);
            
            if(code <= 559 && code >= 500){
                printf("Error!!\n");
                exit(1);
            }
            break;
        }
    }  

    return 0;
}

int readFile(char *fileName, int sockfdReceive ){
    int file;

    if ((file = open(fileName, O_WRONLY | O_CREAT, 0666)) == -1) {
        perror("Error: Couldn't open file");
        return -1;
    }

    char c[1];
    int b;

    while (TRUE){
        b = read(sockfdReceive, &c[0], 1);
        if (b == 0) break;
        if (b < 0){
            close(file);
            return -1;
        }

        b = write(file, &c[0], 1);
        if (b < 0){
            close(file);
            perror("Error: Couldn't write to file");
        }
    }
    
    close(file);

    return 0;
}

int download(struct urlData urlData){

    int sockfd, sockfdReceive;
    
    if(startConnection(urlData.ip, 21, &sockfd) != 0){
        printf("Error: Starting connection\n");
        return -1;
    }

    FILE * readSockect = fdopen(sockfd, "r");
    readServerReply(readSockect);
    
    // credentials
    char command[256];
    sprintf(command, "user %s\n", urlData.user);

    sendCommandToServer(sockfd,command);
    readServerReply(readSockect);

    sprintf(command, "pass %s\n", urlData.password);

    sendCommandToServer(sockfd,command);
    readServerReply(readSockect);

    // set mode
    sprintf(command, "pasv \n");
    sendCommandToServer(sockfd,command);

    //read ip and port 
    char ip[32];
    int port;
    gerIpAndPort(ip, &port, readSockect);
    printf("ip: %s\n",ip);
    printf("port: %i\n", port);

    //open connection to receive data
    if(startConnection(ip, port, &sockfdReceive) != 0){
        printf("Error: Starting connection\n");
        return -1;
    }

    sprintf(command, "retr %s\r\n",urlData.url_path);
    sendCommandToServer(sockfd,command);
    readServerReply(readSockect);

    readFile(urlData.fileName, sockfdReceive);

    //close
    sprintf(command, "quit \r\n");
    sendCommandToServer(sockfd,command);

    return 0;
}

int main(int argc, char **argv) {

    if(argc != 2){
          printf("Incorrect program usage\n"
                "Usage: download ftp://<user>:<password>@<host>/<url>\n");
        exit(1);
    }

    struct urlData urlData;

    if(parseUrlData(argv[1], &urlData) != 0){
        printf("Error: Parsing input\n");
        return 1;
    }
    
    if(download(urlData) == -1){
        printf("Error: Downloading file\n");
        exit(-1);
    }
    
    return 0;
}