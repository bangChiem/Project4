#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 7777

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	char* username;  	
	int clisockfd;		// socket file descriptor
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail = tail->next;
	}
}

// returns 1 on successfull username removal, returns 0 if client cannot be found
void insert_username(char* new_username, int clisockfd){
	new_username[strcspn(new_username, "\n")] = '\0';
	USR *cur = head;
	while (cur != NULL){
		if (cur->clisockfd == clisockfd){
			cur->username = malloc(sizeof(new_username) + 1);
			strncpy(cur->username, new_username, sizeof(new_username) - 1);
			return;
		}
		cur = cur->next;
	}
}

void print_current_clients(){
	USR *cur = head;
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);

	printf("CURRENT CLIENTS\n");
	while(cur != NULL){
		if (cur->username != NULL){
			// get client ip address from client socket file directory
			if (getpeername(cur->clisockfd, (struct sockaddr*)&cliaddr, &clen) < 0){
				cur = cur->next;
				continue;
			}
			printf("%s [ip: %s]\n", cur->username, inet_ntoa(cliaddr.sin_addr));
		}	
		cur = cur->next;
	}
	printf("\n");
}

// given client fd return ip address string
char* getIpAddress(int clifd){
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(clifd, (struct sockaddr*)&cliaddr, &clen) < 0) error("GET IP ADDRESS ERROR Unknown sender!");
	return inet_ntoa(cliaddr.sin_addr);
}

void broadcast(int fromfd, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0){
		return;
	}

	//get send USR
	USR* sender = head;
	while (sender != NULL){
		if (sender->clisockfd == fromfd){
			break;
		}
		sender = sender->next;
	}


	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			char buffer[512];

			// prepare message
			sprintf(buffer, "[%s(%s)] %s", sender->username, getIpAddress(sender->clisockfd), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

void remove_client(int sockfd) {
	USR* cur = head;
	USR *prev = NULL;

	while (cur != NULL) {
		if (cur->clisockfd == sockfd){
			char client_left_msg[50];
			sprintf(client_left_msg, "left the chat room\n");
			broadcast(sockfd, client_left_msg);
			free(cur->username);
			if (prev == NULL){
				head = cur->next; // case for removing head node
				if (cur == tail){
					tail = NULL;
				}
			}
			else {
				prev->next = cur->next; // removing middle/tail node
				if (cur == tail){
					tail = prev; // update tail if needed
				} 
			}
			free(cur);
			return;
			}
		prev = cur;
		cur = cur->next;
	}
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;

	// receive username
	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	// insert username
	insert_username(buffer, clisockfd);

	// TODO send to client server acceptance of username
	strcpy(buffer, "usr_accepted\0");
	nsen = send(clisockfd, buffer, strlen(buffer) + 1, 0);
	if (nsen < 0) error("ERROR send() failed");

	// broadcast to all clients of new client that has joined server
	char client_joined_msg[50];
	sprintf(client_joined_msg, "joined chatroom!\n");
	broadcast(clisockfd, client_joined_msg);

	// print current clients to server
	print_current_clients();

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		if (nrcv > 0 ){
			broadcast(clisockfd, buffer);
		}
	}
	// if client leaves, remove client from linked list and notify all current clients of new client list
	remove_client(clisockfd);
	print_current_clients();
	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		add_tail(newsockfd); // add this new client to the client list

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}

