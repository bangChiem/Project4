#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM 5555
#define FTPort 21

#define COLOR 6 // Total amount of colors
int col[COLOR];
int col_chosen[COLOR];
int col_index = -1;

void init_col(){
    for (int i = 0; i < COLOR; i++)
    {
        col[i] = -1;
    }
}

int gen_random_color(){
    int c;
    while(1){
        c = ((rand() % COLOR));
        if (col[c] == -1){
            col[c] = 0;
            return c+1;
        }
    }
    return 0;
}

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _ROOM {
	int roomNumber;
	int amt_clients;
	struct _ROOM* next;
} ROOM;

typedef struct _USR {
	int msg_col;		// users msg color in chat room
	char* username;  	// users name for chat room
	int clisockfd;		// socket file descriptor
	struct _ROOM* room; // pointer to user's room
	struct _USR* next;	// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

ROOM *room_head = NULL;
ROOM *room_tail = NULL;

ROOM* add_room(int room_number){
	if (room_head == NULL){
		room_head = (ROOM*) malloc(sizeof(ROOM));
		room_head->roomNumber = room_number;
		room_head->amt_clients = 0;
		room_head->next = NULL;
		room_tail = room_head;
		return room_tail;
	} else{
		room_tail->next = (ROOM*) malloc(sizeof(ROOM));
		room_tail->next->roomNumber = room_number;
		room_tail->next->amt_clients = 0;
		room_tail->next->next = NULL;
		room_tail = room_tail->next;
		return room_tail;
	}
}

// search for room
ROOM* search_room(int room_number){
	ROOM* cur = room_head;
	while(cur != NULL){
		if (cur->roomNumber == room_number){
			return cur;
		}
		cur = cur->next;
	}
	return NULL;
}

// print all rooms in server
void print_current_rooms(){
	ROOM* cur = room_head;
	while(cur != NULL){
		cur = cur->next;
	}
}

void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->username = NULL;
		head->room = NULL;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->next = NULL;
		tail->next->username = NULL;
		tail = tail->next;
	}
}

// returns 1 on successfull user to room insertion
int connect_user_to_room(ROOM* room, int clisockfd){
	if (room == NULL){
		error("invalid room");
	}

	USR *cur = head;
	while (cur != NULL){
		if (cur->clisockfd == clisockfd){
			cur->room = room;
			cur->room->amt_clients++;
			return 1;
		}
		cur = cur->next;
	}
	return 0;
}

// returns 1 on successfull username insertion, returns 0 if client cannot be found
int insert_username(char* new_username, int clisockfd){
	new_username[strcspn(new_username, "\n")] = '\0';
	USR *cur = head;
	while (cur != NULL){
		if (cur->clisockfd == clisockfd){
			cur->username = malloc(strlen(new_username) + 1);
			strcpy(cur->username, new_username);
			return 1;
		}
		cur = cur->next;
	}
	return 0;
}

// returns 1 on successfull color change, returns 0 if client cannot be found
int set_user_msg_color(int clisockfd){
	USR *cur = head;
	while (cur != NULL){
		if (cur->clisockfd == clisockfd){
			cur->msg_col = gen_random_color();
			return 1;
		}
		cur = cur->next;
	}
	return 0;
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
			
			printf("%s[ip: %s] ROOM#%3d\n", cur->username, inet_ntoa(cliaddr.sin_addr), cur->room->roomNumber);
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

void broadcast(int fromfd, char* message, int server_msg)
{
	// slice newline char from msg
	message[strcspn(message, "\n")] = '\0';

	// message color of sender
	char sender_msg_color[6];

	// get send USR
	USR* sender = head;
	while (sender != NULL){
		if (sender->clisockfd == fromfd){
			break;
		}
		sender = sender->next;
	}

	if (sender == NULL || sender->room == NULL){
		return;
	}

	// get senders room number
	int sender_room_number = sender->room->roomNumber;

	// get sender msg color / if broadcast is server message default color to white
	if (server_msg == 1){
		sprintf(sender_msg_color, "\e[0m");
	}
	else{
		sprintf(sender_msg_color, "\e[3%dm", sender->msg_col);
	}

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {
		// check if sender is in the same room as cur
		if(cur->room->roomNumber == sender_room_number){
			// check if cur is not the one who sent the message
			if (cur->clisockfd != fromfd && cur->username != NULL) {
				// prepare message
				char buffer[512]={0};
				sprintf(buffer, "%s%s(%s)] %s\e[0m\n", sender_msg_color, sender->username, getIpAddress(sender->clisockfd), message);
				// send!
				int nsen = send(cur->clisockfd, buffer, strlen(buffer), 0);
				if (nsen != strlen(buffer)) error("ERROR send() failed");
			}
		}


		cur = cur->next;
	}
}

void remove_client(int sockfd) {
    USR* cur = head;
    USR *prev = NULL;

    while (cur != NULL) {
        if (cur->clisockfd == sockfd){
            // Skip color freeing if not set
            if (cur->username != NULL) {
                col[cur->msg_col -1] = -1;
            }

            // Only broadcast if the client was fully initialized
            if (cur->username != NULL && cur->room != NULL) {
                char client_left_msg[50];
                sprintf(client_left_msg, "left the chat room\n");
                broadcast(sockfd, client_left_msg, 1);
            }

			// Skip room count reduction if no room assigned
            if (cur->room != NULL) {
                cur->room->amt_clients--;
            }
           
            if (cur->username != NULL) {
                free(cur->username);
            }
           
            if (prev == NULL){
                head = cur->next;
                if (cur == tail){
                    tail = NULL;
                }
            }
            else {
                prev->next = cur->next;
                if (cur == tail){
                    tail = prev;
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

// retrieve room information and save to buffer
int get_room_info(char* buffer){
	if (room_head == NULL){
		sprintf(buffer, "NO ROOMS");
		return 0;
	}
	else{
		ROOM* cur = room_head;
		while(cur != NULL){
			char line[200];
			snprintf(line, sizeof(line), "\tRoom %d: %d person\n", cur->roomNumber, cur->amt_clients);
			strncat(buffer, line, 512 - strlen(buffer) - 1);
			cur = cur->next;
		}
		return 1;
	}
}

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[512];
	int nsen, nrcv = 0;

	// receive from client room request
	int net_number;
	recv(clisockfd, &net_number, sizeof(net_number), 0);
	int room_number = ntohl(net_number);

	// if user requested to create new room
	ROOM* client_room;
	int new_room_number;
	if (room_number == -1){
		do{
			new_room_number = rand() % 1000;
		} while(search_room(new_room_number) != NULL && new_room_number != 0);

		// create new room struct
		client_room = add_room(new_room_number);
		
		// send to client that room was created successfully
		net_number = htonl(new_room_number);
		nsen = send(clisockfd, &net_number, sizeof(net_number), 0);
		if (nsen < 0) error("ERROR send() failed");
	}
	// retrieve all chat room and send to client
	else if (room_number == -2){
		memset(buffer, 0, 512);
		int room_info_status = get_room_info(buffer);
		nsen = send(clisockfd, buffer, strlen(buffer), 0);
		if (nsen < 0) error("ERROR send() failed");
		remove_client(clisockfd);
		close(clisockfd);
		return NULL;
	}
	// if not search current rooms for matching room #
	else{
		client_room = search_room(room_number);
		if (client_room != NULL){
			// send to client that room exists
			net_number = htonl(room_number);
			nsen = send(clisockfd, &net_number, sizeof(net_number), 0);
			if (nsen < 0) error("ERROR send() failed");
		}
		else{
			// send to client that room does not exist
			net_number = htonl(-1);
			nsen = send(clisockfd, &net_number, sizeof(net_number), 0);
			remove_client(clisockfd);
			close(clisockfd);
			return NULL;
		}
	}

	// receive username
	while(nrcv == 0){
		memset(buffer, 0, 512);
		nrcv = recv(clisockfd, buffer, 512, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	add_tail(clisockfd); // add this new client to the client list

	// insert username
	if (insert_username(buffer, clisockfd) == 0){
		error("failed to insert users username");
	};

	// set user message color
	if(set_user_msg_color(clisockfd) == 0){
		error("failed to find user and change message color");
	}

	// add user to room
	if (connect_user_to_room(client_room, clisockfd) == 0){
		error("failed to find user and change message color");
	}
	
	// Send to client server acceptance of username
	const char* confirm_msg = "CONNECTED\n";
	nsen = send(clisockfd, confirm_msg, strlen(confirm_msg), 0);
	if (nsen < 0) error("ERROR send() failed");
	if (nsen == 0){
		printf("ERROR invalid username entered");
		return NULL;
	} 

	// broadcast to all clients of new client that has joined server
	char client_joined_msg[50];
	sprintf(client_joined_msg, "joined chatroom!\n");
	broadcast(clisockfd, client_joined_msg, 1);

	// print current clients to server
	print_current_clients();

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		memset(buffer, 0, 512);
		nrcv = recv(clisockfd, buffer, 512, 0);
		if (nrcv < 0) error("ERROR recv() failed");

		if (nrcv == 1) {
			break;
		}

		if (nrcv > 0){
			//TODO
			//if buffer == send filename
				//if client wants file
					//call ftp function
			//else
			broadcast(clisockfd, buffer, 0);
		}
	}
	// if client leaves, remove client from linked list and notify all current clients of new client list
	remove_client(clisockfd);
	print_current_clients();
	close(clisockfd);
	//-------------------------------

	return NULL;
}

void *file_transfer(void *args){
	pthread_detach(pthread_self());
	//extract room number, filename, and client sending form args

	//add mutex lock so only 1 file is transfered at a time
	//create a serverside temporary cpy of file

	//broadcast asking clients in room if they would like the file

	//send to thoses who accept

	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	// initialize random seed time and color array
	srand(time(0));
	init_col();

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

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	return 0; 
}
