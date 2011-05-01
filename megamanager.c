// This is the mega comet manager
// See the 'readme' for a good run-down of how this fits into the picture of things
// The gist of it is that the app tells the manager when it has an outgoing message, and this
// then tells the correct worker to send the message. This waits for the workers to connect to it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "khash.h"
#include <ev.h>

#define MANAGER_PORT_NO 9000 // The port we are to listen for the workers
#define LISTEN_BACKLOG 1024 // The number of pending connections that can be queued up at any one time 
#define WORKERS 8 // The number of workers
#define MAX_CONNS 16 // We need to cater for N connections. Usually 8 workers + 1 (or more) app connections
#define MAX_CLIENT_ID_LEN 128 // Length of the client id's
#define MAX_MESSAGE_LEN 1024 // Length of the message
#define BUFFER_SIZE 2048 // Size of the chunks we read incoming commands in. Should be big enough for a full command

// Useful utilities
typedef unsigned char byte;

// Globals (yuck!)
int managerSd; // The listening socket file descriptors
struct ev_loop *libEvLoop; // The main libev loop. Global so that we don't have to pass it around everywhere
typedef struct connection {
	int socket; // File descriptor
	int readStatus; // For parsing the input bytes
	int workerNo; // Which worker number it is (0-7) or -1 if not a worker
	byte appClientId[MAX_CLIENT_ID_LEN+1]; // The client id for an incoming message from the app (+1 for null term)
	int appClientIdLen;
	byte appMessage[MAX_MESSAGE_LEN+1]; // The message for an incoming message from the app (+1 for null term)
	int appMessageLen;
} connection;
connection conn[MAX_CONNS]; // Just using an array not a hash because its quicker for small lists
int conns = 0;
byte forwardingBuf[MAX_CLIENT_ID_LEN+MAX_MESSAGE_LEN+3]; // For the message forwarding to use

void newConnectionCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void readCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);

// Open the listening socket for incoming worker connections
void openManagerSocket(void) {
	// Open the socket file descriptor
	managerSd = socket(PF_INET, SOCK_STREAM, 0);
	if (managerSd < 0) {
		perror("comet manager socket error");
		exit(1);
	}

	// This kills "Address already in use" error message. This happens because we close the sockets
	// first, not letting the clients close them
	int tr=1;
	if (setsockopt(managerSd,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}

	// Bind the socket to the address
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MANAGER_PORT_NO);
	addr.sin_addr.s_addr = INADDR_ANY;
	int bindResult = bind(managerSd, (struct sockaddr*) &addr, sizeof(addr));
	if (bindResult < 0) {
		perror("bind error");
		exit(1);
	}

	// Start listing on the socket
	int listenResult = listen(managerSd, LISTEN_BACKLOG);
	if (listenResult < 0) {
		perror("listen error");
		exit(1);
	}

	puts("Socket opened");
}

// All the setup stuff goes here
void setup() {
	puts("MegaComet Manager");
	openManagerSocket();
}

// The main libev loop
void run() {
	// use the default event loop unless you have special needs
	libEvLoop = ev_default_loop(0);

	// Initialize and start a watcher to accepts client requests
	struct ev_io managerPortWatcher;
	ev_io_init(&managerPortWatcher, newConnectionCallback, managerSd, EV_READ);
	ev_io_start(libEvLoop, &managerPortWatcher);

	puts("Libev initialised, starting...");

	// Start infinite loop
	ev_loop(libEvLoop, 0);
}

// Everyone's favourite function!
int main() {
	setup();
	run();
	return 0;
}

// Accept client requests
void newConnectionCallback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	if (EV_ERROR & revents) {
		puts("got invalid event");
		return;
	}

	// Accept client request
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sd < 0) {
		puts("accept error");
		return;
	}

	// Set it up in the connections list
	if (conns >= MAX_CONNS) {
		// Too many
		puts("Too many connections");
		close(client_sd);
		return;
	}
	bzero(&conn[conns], sizeof(connection));
	conn[conns].socket = client_sd;
	conn[conns].readStatus = 0;
	conn[conns].workerNo = -1;
	conns++;

	// Initialize and start watcher to read client requests
	struct ev_io *watcherRead = calloc (1, sizeof(struct ev_io));
	ev_io_init(watcherRead, readCallback, client_sd, EV_READ);
	ev_io_start(loop, watcherRead);
}

// Close a connection and free the memory associated
void closeConnection(struct ev_io *watcher, int iconn) {
	ev_io_stop(libEvLoop, watcher); // Tell libev to stop following it
	close(watcher->fd); // Close the socket

	// Remove the client status from the array
	if (iconn < conns-1) { // Do we need to shuffle the last entry to this position?		
		memcpy(&conn[iconn], &conn[conns-1], sizeof(connection));
	}
	conns--;

	free(watcher); // Free the watcher (this is last because the fd is used above, after ev_io_stop)
}

// The app connection just received a message to forward to a worker
void forwardMessage(int iconn) {
	// Figure out which worker to send it to
	conn[iconn].appClientId[conn[iconn].appClientIdLen] = 0; // Put on the null terminator
	khint_t hash = kh_str_hash_func((char*)conn[iconn].appClientId); // Do a crypto hash on the client
	int worker = hash % WORKERS; // Use the hash value to determine which worker they'll be on

	// Now see if we can find that worker, hopefully it's connected to us
	int socket = -1;
	for (int i=0;i<conns;i++) {
		if (conn[i].workerNo == worker) {
			socket = conn[i].socket;
			break;
		}
	}
	if (socket<0) {
		printf ("Got a message for worker %d but it's not connected, dropped\r\n", worker);
		return;
	}

	// Now forward the message to that worker
	// Firstly compile it
	int off=0;
	forwardingBuf[off]=2; // 2 means 'message'
	off++;
	memcpy(forwardingBuf+off, conn[iconn].appClientId, conn[iconn].appClientIdLen); // The client id
	off += conn[iconn].appClientIdLen;
	forwardingBuf[off]=0; // Null terminate the client id
	off++;
	memcpy(forwardingBuf+off, conn[iconn].appMessage, conn[iconn].appMessageLen); // The message
	off += conn[iconn].appMessageLen;
	forwardingBuf[off]=0; // Null terminate the message
	off++;

	// Now send it
	write(socket, forwardingBuf, off);
}

/* Read client message */
void readCallback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	byte buffer[BUFFER_SIZE];
	size_t read;

	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	// Find this socket in the conns list.
	int iconn = -1;
	for (int i=0;i<conns;i++) {
		if (conn[i].socket == watcher->fd) {
			iconn = i;
			break;
		}
	}
	if (iconn < 0) {
		puts ("unknown file descriptor");
		return;
	}

	// Receive message from client socket
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);
	
	if (read < 0) {
		puts ("read error");
		closeConnection(watcher, iconn);
		return;
	}
	if (read == 0) {
		// Stop and free watcher if client socket is closing
		if (conn[iconn].workerNo < 0) {
			puts("App connection closing nicely");
		} else {
			printf("Worker %d connection closing nicely\r\n", conn[iconn].workerNo);
		}
		closeConnection(watcher, iconn); // TODO is the socket close in this function necessary since the other side closed it anyway?
		return;
	}
	// Go through the bytes read and parse what the client is sending us
	for (int i=0; i<read; i++) {
		if (conn[iconn].readStatus==0) {
			if (buffer[i]==1) { // Start of the 'my worker # is X'
				conn[iconn].readStatus = 100;
				continue;				
			}		
			if (buffer[i]==2) { // Start of the app sending a message
				conn[iconn].readStatus = 200;
				conn[iconn].appClientIdLen = 0;
				conn[iconn].appMessageLen = 0;
				continue;				
			}		
		}
		if (conn[iconn].readStatus==100) { // We are waiting for the worker #
			conn[iconn].workerNo = buffer[i];
			printf("Worker %d connected\r\n", conn[iconn].workerNo);
			conn[iconn].readStatus = 0;
			continue;
		}
		if (conn[iconn].readStatus==200) { // We are waiting for the app sending a client id
			if (buffer[i]==0) {
				conn[iconn].readStatus=201; // Now wait for the message	
				continue;
			} else {
				if (conn[iconn].appClientIdLen < MAX_CLIENT_ID_LEN) {
					conn[iconn].appClientId[conn[iconn].appClientIdLen] = buffer[i];
					conn[iconn].appClientIdLen ++;
					continue;
				} else {
					// Buffer overrun on the client id, so put the error and kill this connection todo
					puts("Buffer overrun on the client id");
					continue;
				}
			}
		}
		if (conn[iconn].readStatus==201) { // We are waiting for the app sending a message
			if (buffer[i]==0) {
				forwardMessage(iconn); // Send the message to the correct worker
				conn[iconn].readStatus=0; // Now wait for the next command	
				continue;
			} else {
				if (conn[iconn].appMessageLen < MAX_MESSAGE_LEN) {
					conn[iconn].appMessage[conn[iconn].appMessageLen] = buffer[i];
					conn[iconn].appMessageLen ++;
					continue;
				} else {
					// Buffer overrun on the message, so put the error and kill this connection todo
					puts("Buffer overrun on the message");
					continue;
				}
			}
		}
		// If it got to the end of the loop here, then the client has sent a malformed message so lets reset the parser
		conn[iconn].readStatus = 0;
	} // end of the for loop
}
