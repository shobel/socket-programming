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
#include <sys/wait.h>
#include <signal.h>
#include <ctime>

#define PORT "4064"  // 3300+764=4064 
#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 1000 // max number of bytes we can get at once 
#define MSGSIZE 100
#define TRANSACTION_ID_MAX 255 
#define DISCOVERY 0
#define OFFER 1
#define REQUEST 2
#define ACK 3

void sigchld_handler(int s) {
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int generateTransactionId(){
    return rand()%(TRANSACTION_ID_MAX - 0 + 1) + 0;
}

void generateIP(char* ip, int trans_id){
	int n1 = rand()%(TRANSACTION_ID_MAX - 0 + 1) + 0;
	int n2 = rand()%(TRANSACTION_ID_MAX - 0 + 1) + 0;
	int n3 = rand()%(TRANSACTION_ID_MAX - 0 + 1) + 0;
	snprintf(ip, MSGSIZE, "%d.%d.%d.%d", trans_id, n1, n2, n3);
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

    char ip[count+1];
    ip[count+1] = '\0';
    int index = 0;
    for (i = count-1; i >= 0; i--){
        ip[index] = backwardsip[i];
        index++;
    }
    ipPointer = ip;
}

void createMessage(int phase, int trans_id, char* message, char* newmsg){
    snprintf(newmsg, MSGSIZE, "%d:%d:%s", phase, trans_id, message);
}

int getTransactionIdFromMessage(char* msg){
	char idString[3];
	int count = 0;
	for (int i = 2; msg[i] != ':'; i++){
		idString[count] = msg[i];
		count++;
	}
	return atoi(idString);
}

int main(void){
    int sockfd, new_fd;  // listen on sockfd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET_ADDRSTRLEN];
    int rv;
    int numChildren = 0;

    int numbytes;
    char buf[MAXDATASIZE];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // get address info of localhost (NULL node) and port
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("SERVER: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("SERVER: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "SERVER: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("SERVER: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("SERVER: got connection from %s on port %s\n", s, PORT);
        numChildren++;
        if (!fork()) {
            char msg[MSGSIZE];
            char ip[MSGSIZE];
            int trans_id;
            char* ip2;

            srand(numChildren);
        	close(sockfd);
        	//Receive message
        	while ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) != -1) {
   		 		//Determine phase
    			int phase = atoi(&buf[0]);
    			if (phase == DISCOVERY){
    				//DISCOVERY, need to send OFFER
    				trans_id = getTransactionIdFromMessage(buf);
    				printf("SERVER: received DISCOVERY message with transaction id %d\n", trans_id);
    				generateIP(ip, trans_id); 
    				trans_id = generateTransactionId();
    				createMessage(OFFER, trans_id, ip, msg);
    				if (send(new_fd, msg, MSGSIZE, 0) == -1) {
       	      	 	  	printf("SERVER: offer send error\n");
       		 		}
        			printf("SERVER: sent OFFER ip address %s with transaction id %d\n", ip, trans_id);
    			} else if (phase == REQUEST){
    				//REQUEST, need to send ACK
    				trans_id = getTransactionIdFromMessage(buf);
    				getIpFromMessage(buf, ip2);
    				printf("SERVER: received REQUEST message for ip %s with transaction id %d\n", ip, trans_id);
					trans_id = generateTransactionId();
    				createMessage(ACK, trans_id, ip2, msg);
    				char newmsg[] = "ACK";
    				if (send(new_fd, newmsg, MSGSIZE, 0) == -1) {
        	        	printf("SERVER: ack send error\n");
        			}
        			printf("SERVER: sending 'ACK' response\n");
        			break;
    			} 
    		}
            close(new_fd);
            exit(0);
    	}
    	close(new_fd);
    }

    return 0;
}
