#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 7777
pthread_mutex_t send_recv_lock;
pthread_cond_t send_recv_cond;

int username_sent = 0;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	pthread_mutex_lock(&send_recv_lock);
	while(username_sent == 0){
		pthread_cond_wait(&send_recv_cond, &send_recv_lock);
	}
	pthread_mutex_unlock(&send_recv_lock);
	pthread_detach(pthread_self());
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n;

	// wait for server to send user accepted handshake message
	memset(buffer, 0, 512);
	n = recv(sockfd, buffer, 512, 0);
	if (n <= 0 || strcmp(buffer, "CONNECTED\n") != 0){
		close(sockfd);
		error("Server did not accept username");
	}

	// receive message that was broadcasted and print in client terminal
	while (n > 0) {
		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");
		printf("%s", buffer);
	}
	return NULL;
}

void* thread_main_send(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// prompt user for username and send to server
	char buffer[512];
	int n;
	printf("Type your username: ");
	memset(buffer, 0, 512);
	fgets(buffer, 512, stdin);
	n = send(sockfd, buffer, strlen(buffer), 0);
	if (n < 0) error("ERROR writing to socket");


	// unlock receive thread to receive server handshake
	username_sent = 1;
	pthread_cond_signal(&send_recv_cond);
	pthread_mutex_unlock(&send_recv_lock);
	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		// printf("\nPlease enter the message: ");
		
		// allow user to send messages to server to be broadcasted to other clients
		memset(buffer, 0, 512);
		fgets(buffer, 512, stdin);
		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");
		if (n == 1){
			break;
		} // we stop transmission when user type empty string
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (pthread_mutex_init(&send_recv_lock, NULL)){
		error("mutex lock failed to intialize");
	}

	if (pthread_cond_init(&send_recv_cond, NULL) != 0){
		error("pthread condition failed to initialize");
	}

	if (argc < 2) error("Please speicify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	if (pthread_create(&tid1, NULL, thread_main_send, (void*) args) != 0){
		error("ERROR: send thread failed to create");
	};

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	if (pthread_create(&tid2, NULL, thread_main_recv, (void*) args) != 0){
		error("ERROR recv thread failed to create");
	};

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	close(sockfd);
	pthread_mutex_destroy(&send_recv_lock);
	pthread_cond_destroy(&send_recv_cond);

	return 0;
}
