#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "4064"  // 3300+764=4064 
#define MAXDATASIZE 1000 // max number of bytes we can get at once
#define TRANSACTION_ID_MAX 255 
#define MSGSIZE 100
#define DISCOVERY 0
#define OFFER 1
#define REQUEST 2
#define ACK 3

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int generateTransactionId(){
    return rand()%(TRANSACTION_ID_MAX - 0 + 1) + 0;
}

void createMessage(int phase, int trans_id, char* message, char* newmsg){
    snprintf(newmsg, MSGSIZE, "%d:%d:%s", phase, trans_id, message);
}

void getIpFromMessage(char* buf, char* &ipPointer){
    char backwardsip[MSGSIZE];
    int count = 0;
    int msglength = strlen(buf);
    int i = 0;
    for (i = msglength-1; buf[i] != ':'; i--){
        backwardsip[count] = buf[i];
        count++;
    }
    char ip[count];
    int index = 0;
    for (i = count-1; i >= 0; i--){
        ip[index] = backwardsip[i];
        index++;
    }
    ipPointer = ip;
}

int getTransactionIdFromMessage(char* msg){
    char idString[4];
    int count = 0;
    for (int i = 2; msg[i] != ':'; i++){
        idString[count] = msg[i];
        count++;
    }
    idString[3] = '\0';
    return atoi(idString);
}

int main(int argc, char *argv[]){
    srand(time(0));
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET_ADDRSTRLEN];

    const char* hostname;
    if (argc != 2) {
        hostname = "127.0.0.1";
        fprintf(stderr,"CLIENT: No server ip specified in first argument - using 127.0.0.1\n");
        //exit(1);
    } else {
        hostname = argv[1];
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // get address info of localhost (NULL node) and port 
    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("CLIENT: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("CLIENT: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "CLIENT: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("CLIENT: connecting to %s on port %s\n", s, PORT);

    freeaddrinfo(servinfo); // all done with this structure

    //PHASES
    int trans_id;
    char msg[MSGSIZE];

    //send DISCOVERY
    trans_id = generateTransactionId();
    char content[1] = "";
    createMessage(DISCOVERY, trans_id, content, msg);
    if (send(sockfd, msg, MSGSIZE, 0) == -1){
        perror("error: client sending");
    }   
    printf("CLIENT: sending DISCOVERY message with transaction id %d\n", trans_id);
    usleep(5000000);
    //receive OFFER 
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';
    char* ip;
    getIpFromMessage(buf, ip);
    trans_id = getTransactionIdFromMessage(buf);
    printf("CLIENT: received OFFER response with transaction id %d. IP offered is %s\n", trans_id, ip);

    //send REQUEST
    trans_id = generateTransactionId();
    createMessage(REQUEST, trans_id, ip, msg);
    if (send(sockfd, msg, MSGSIZE, 0) == -1){
        perror("error: client sending");
    }   
    printf("CLIENT: sending REQUEST for ip %s with transaction id %d\n", ip, trans_id);
    usleep(5000000);
    //ACK received
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    char ack[] = "ACK";
    if (strcmp(ack, buf) == 0){
        printf("CLIENT: received ACK response with no transaction id. IP is now set to %s\n", ip);
    } else {
        printf("CLIENT: no ACK response received. IP request failed\n");
    }
    usleep(2000000);
    close(sockfd);

    return 0;
}

