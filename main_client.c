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

#define PORT_NUM 5555
#define FTPort 21

pthread_mutex_t send_recv_lock;
pthread_cond_t send_recv_cond;
pthread_cond_t room_req_cond;
pthread_cond_t room_serv_resp_cond;

int username_sent = 0;
int server_room_response = 0;
int room_req_sent = 0;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
	int new_room_req;
	char *req_server_ip;
} ThreadArgs;

//Function for dealing with file send command
void* send_file(void* args){
	pthread_detach(pthread_self());
	
	//Connect to socket via port FTport
	
	//Tell server it will recieve transfer

	//Extract from args file name
	
	//call fopen on the file

	//read the file and send its contents to the server
		//TODO add mutex lock so 2 files cannot be sent at the same time
	pthread_exit(0);
}

void* recv_file(void* args){
	pthread_detach(pthread_self());
	//Connect to socket via port FTport

	//Extract filename

	//call fopen

	//read from server and write to file
		//Add mutex lock so 2 files cannot be accepted at once
	pthread_exit(0);
}

void* thread_main_recv(void* args)
{
	char buffer[512];
	int n;
	int room_req = ((ThreadArgs*) args)->new_room_req;
	char server_ip_address[20];
	strcpy(server_ip_address, ((ThreadArgs*) args)->req_server_ip);
	pthread_detach(pthread_self());
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	
	// wait for room_req_sent
	pthread_mutex_lock(&send_recv_lock);
	while(room_req_sent == 0){
		pthread_cond_wait(&room_req_cond, &send_recv_lock);
	}
	pthread_mutex_unlock(&send_recv_lock);
	free(args);

	// receive server response
	int net_number;
	recv(sockfd, &net_number, sizeof(net_number), 0);
	int rcv_room_response = ntohl(net_number);

	if (rcv_room_response < 0){
		server_room_response = -1;
		pthread_cond_signal(&room_serv_resp_cond);
		pthread_mutex_unlock(&send_recv_lock);
		printf("Room number does not exist\n");
		return NULL;
	}
	else if (rcv_room_response > 0){
		server_room_response = 1;
		pthread_cond_signal(&room_serv_resp_cond);
		pthread_mutex_unlock(&send_recv_lock);
	}
	else{
		printf("INVALID ROOM SERVER RESPONSE: %d", rcv_room_response);
	}

	pthread_mutex_lock(&send_recv_lock);
	while(username_sent == 0){
		pthread_cond_wait(&send_recv_cond, &send_recv_lock);
	}
	pthread_mutex_unlock(&send_recv_lock);

	// notify client of successful connection to server and room
	if (room_req == -1){
		printf("Connected to %s with new room number %d\n", server_ip_address, rcv_room_response);
	}
	else{
		printf("Connected to %s with room number %d\n", server_ip_address, rcv_room_response);
	}

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
		//TODO check if message recieved was to recieve a file
		//if buffer = SEND filename
			//ask if would like to recieve file
			//if yes initiate recv_file thread pass filename
		//else
		printf("%s", buffer);
	}
	return NULL;
}

void* thread_main_send(void* args)
{
	int n;
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	int room_req = ((ThreadArgs*) args)->new_room_req;
	free(args);

	// send new room request to server
	int net_number;
	if (room_req == -1){
		net_number = htonl(room_req);
		send(sockfd, &net_number, sizeof(net_number), 0);

	}
	// or send to server request to join room #
	else{
		net_number = htonl(room_req);
		send(sockfd, &net_number, sizeof(net_number), 0);
	}

	// signal to receive thread that server has sent response
	room_req_sent = 1;
	pthread_mutex_lock(&send_recv_lock);
	pthread_cond_signal(&room_req_cond);
	pthread_mutex_unlock(&send_recv_lock);

	// Wait for recv thread to receive servers response to room request 
	pthread_mutex_lock(&send_recv_lock);
	while(server_room_response == 0){
		pthread_cond_wait(&room_serv_resp_cond, &send_recv_lock);
	}
	pthread_mutex_unlock(&send_recv_lock);

	if (server_room_response == 1){
		
		// prompt user for username and send to server
		char buffer[512];
		printf("Type your username: ");
		memset(buffer, 0, 512);
		fgets(buffer, 512, stdin);
		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) error("ERROR writing to socket");


		// unlock receive thread to receive server handshake
		username_sent = 1;
		pthread_mutex_lock(&send_recv_lock);
		pthread_cond_signal(&send_recv_cond);
		pthread_mutex_unlock(&send_recv_lock);

		while (1) {
			// You will need a bit of control on your terminal
			// console or GUI to have a nice input window.
			// printf("\nPlease enter the message: ");
			
			// allow user to send messages to server to be broadcasted to other clients
			memset(buffer, 0, 512);
			fgets(buffer, 512, stdin);
			//TODO
			//if buffer = sendfilename
				//start send ftp
			//else
			n = send(sockfd, buffer, strlen(buffer), 0);
			if (n < 0) error("ERROR writing to socket");
			if (n == 1){
				break;
			} // we stop transmission when user type empty string
		}
	}
	return NULL;
}

int get_serv_room_info(int sockfd){
	int n;
	char buffer[512];
	// send to server flag for requst for room info
	int net_number = htonl(-2);
	send(sockfd, &net_number, sizeof(net_number), 0);
	memset(buffer, 0, 512);

	n = recv(sockfd, buffer, 512, 0);
	if (n < 0) error("ERROR recv() failed");

	// check for a empty room
	if (strcmp(buffer, "NO ROOMS") == 0){
		return 0;
	}

	printf("%s", buffer);
	return 1;

}

int main(int argc, char *argv[])
{
	if (pthread_mutex_init(&send_recv_lock, NULL)){
		error("mutex lock failed to intialize");
	}

	if (pthread_cond_init(&send_recv_cond, NULL) != 0){
		error("pthread send_recv_cond condition failed to initialize");
	}

	if (pthread_cond_init(&room_req_cond, NULL) != 0){
		error("pthread room_req_cond condition failed to initialize");
	}

	if (pthread_cond_init(&room_serv_resp_cond, NULL) != 0){
		error("pthread room_req_cond condition failed to initialize");
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

	char* req_serv_ip = argv[1];
	int chat_room_req;

	// if client did not specify chat room retrieve rooms from server
	if (argc < 3){
		char buffer[512];
		int room_info_status = get_serv_room_info(sockfd);

		// Reconnect to server
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) error("ERROR opening socket");
		int status = connect(sockfd, 
		(struct sockaddr *) &serv_addr, slen);
		if (status < 0) error("ERROR connecting");
		username_sent = 0;
		server_room_response = 0;
		room_req_sent = 0;

		if (room_info_status == 1){
			printf("Choose the room number or type [new] to create a new room: ");
			memset(buffer, 0, 512);
			fgets(buffer, 512, stdin);
			if (strcmp(buffer, "new\n") == 0){
				printf("creating new room\n");
				chat_room_req = -1;
			}
			else{
				printf("attempting to join room %s\n", buffer);
				chat_room_req = atoi(buffer);
			}
		}
		else if (room_info_status == 0){
			printf("NO ROOMS creating new room\n");
			chat_room_req = -1;
		}
		else{
			error("Undefined room info status returned");
		}
	}
	else{
		if (strcmp(argv[2], "new") == 0){
			chat_room_req = -1;
		}
		else{
			chat_room_req = atoi(argv[2]);
		}
	}

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;

	// pass client socket fd, server ip and room number request to threads
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->new_room_req = chat_room_req;
	args->clisockfd = sockfd;
	args->req_server_ip = malloc(strlen(req_serv_ip));
	strcpy(args->req_server_ip, req_serv_ip);
	// pass client socket fd, server ip and room number request to threads
	if (pthread_create(&tid1, NULL, thread_main_send, (void*) args) != 0){
		error("ERROR: send thread failed to create");
	};


	// recreate and pass client socket fd, server ip and room number request to threads
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	// pass client socket fd, server ip and room number request to threads
	args->clisockfd = sockfd;
	args->req_server_ip = malloc(strlen(req_serv_ip));
	strcpy(args->req_server_ip, req_serv_ip);

	if (pthread_create(&tid2, NULL, thread_main_recv, (void*) args) != 0){
		error("ERROR recv thread failed to create");
	};

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	close(sockfd);
	pthread_mutex_destroy(&send_recv_lock);
	pthread_cond_destroy(&send_recv_cond);
	pthread_cond_destroy(&room_req_cond);
	pthread_cond_destroy(&room_serv_resp_cond);

	return 0;
}
