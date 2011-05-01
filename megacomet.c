// Worth looking at:
// https://github.com/coolaj86/libev-examples/blob/master/src/unix-echo-server.c
// http://codefundas.blogspot.com/2010/09/create-tcp-echo-server-using-libev.html
// http://en.wikipedia.org/wiki/Berkeley_sockets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <ev.h>
#include "khash.h"

#define COMET_BASE_PORT_NO 8000 // Which port range are we listening on for clients
#define MAX_CLIENT_ID_LEN 128 // Length of the client id's
#define MAX_MESSAGE_LEN 1024 // Length of the message
#define BUFFER_SIZE 2048 // Size of the chunks we read incoming commands in. Should be big enough for a full command
#define LISTEN_BACKLOG 1024 // The number of pending connections that can be queued up at any one time 
#define FIRST_LINE_SIZE 60 // The length of the first line allowed (the GET line) - each byte here equals a meg when multiplied by 1m!
#define MANAGER_PORT_NO 9000 // The port we are to connect to the manager to talk to us on

// Useful utilities
typedef unsigned char byte;

// Globals
int cometSd, managerSd; // The listening socket file descriptor
struct ev_loop *libEvLoop; // The main libev loop. Global so that we don't have to pass it around everywhere, slowly pushing and popping it to the stack

// For the status of each connection, we have a hash that goes from the socket file descriptor to the below struct:
struct clientStatus {
	char clientId[MAX_CLIENT_ID_LEN+1]; // Eg will be 'myClientId' for: GET /myClientId.js?c=cachekiller HTTP/1.1
	int clientIdLen; // Length of the client id
	int readStatus; // 0=nothing, waiting for '/'
	// First line: 10=found '/', reading client id
	// 20=found '.', reading the rest of the first line
	// First line: 100=found '\r', 200=found '\n' 
	// Reading headers: 200, 300=found '\r', 400='\n', 500=2nd '\r', 1000=found 2nd '\n'
	// Ready to respond: 1000
	int bytes; // The number of bytes read so far
};
KHASH_MAP_INIT_INT(clientStatuses, struct clientStatus*); // Creates the macros for dealing with this hash
khash_t(clientStatuses) *clientStatuses; // The hash table

void newConnectionCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

// Open the listening socket for incoming comet connections
void openCometSocket(void) {
	// Open the socket file descriptor
	cometSd = socket(PF_INET, SOCK_STREAM, 0);
	if (cometSd < 0) {
		perror("comet socket error");
		exit(1);
	}

	// This kills "Address already in use" error message. This happens because we close the sockets
	// first, not letting the clients close them
	int tr=1;
	if (setsockopt(cometSd,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int)) == -1) {
	    perror("setsockopt");
	    exit(1);
	}

	// Bind the socket to the address
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(COMET_BASE_PORT_NO);
	addr.sin_addr.s_addr = INADDR_ANY;
	int bindResult = bind(cometSd, (struct sockaddr*) &addr, sizeof(addr));
	if (bindResult < 0) {
		perror("bind error");
		exit(1);
	}

	// Start listing on the socket
	int listenResult = listen(cometSd, LISTEN_BACKLOG);
	if (listenResult < 0) {
		perror("listen error");
		exit(1);
	}

	puts("Socket opened");
}

// Open the connection to the manager
void openManagerSocket(void) {
	// Open the socket file descriptor
	cometSd = socket(PF_INET, SOCK_STREAM, 0);
	if (cometSd < 0) {
		perror("manager socket error");
		exit(1);
	}

	// Build the address of the manager
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(COMET_BASE_PORT_NO);
	addr.sin_addr.s_addr = INADDR_LOOPBACK;

	// Connect to the manager
	puts ("Connecting to manager...");
	int connectResult = connect(cometSd, (struct sockaddr*) &addr, sizeof addr);
	if (connectResult < 0) {
		perror("Could not connect to manager. Start the manager first!");
		exit(1);
	}

	puts("Manager connected");
}

// The main libev loop
void run() {
	// use the default event loop unless you have special needs
	libEvLoop = ev_default_loop(0);

	// Initialize and start a watcher to accepts client requests
	struct ev_io cometPortWatcher;
	ev_io_init(&cometPortWatcher, newConnectionCallback, cometSd, EV_READ);
	ev_io_start(libEvLoop, &cometPortWatcher);

	puts("Ready");

	// Start infinite loop
	ev_loop(libEvLoop, 0);
}

// Initialise the hash tables that are needed
void initHashes() {
	clientStatuses = kh_init(clientStatuses); // Malloc the hash
}

// All the setup stuff goes here
void setup() {
	initHashes();
	openCometSocket();
	//openManagerSocket();
}

// All the shutdown stuff goes here. Is it really worth bothering to clean up memory just prior to exit?
void shutDown() {
	close(cometSd);
	close(managerSd);
	kh_destroy(clientStatuses, clientStatuses); // Free it all
	// Todo clean up the libev stuff
}

// Everyone's favourite function!
int main() {
	setup();
	run();
	shutDown();
	return 0;
}

/* Accept client requests */
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

	// Add this client's status to the hash
	struct clientStatus *newStatus = calloc(1, sizeof(struct clientStatus));
	int ret;
	khiter_t k = kh_put(clientStatuses, clientStatuses, client_sd, &ret);
	kh_value(clientStatuses, k) = newStatus;

	// Initialize and start watcher to read client requests
	struct ev_io *watcherRead = calloc (1, sizeof(struct ev_io));
	ev_io_init(watcherRead, read_cb, client_sd, EV_READ);
	ev_io_start(loop, watcherRead);
}

// Close a connection and free the memory associated
void closeConnection(struct ev_io *watcher) {
	ev_io_stop(libEvLoop, watcher); // Tell libev to stop following it
	close(watcher->fd); // Close the socket
	// Remove the client status from the hash
	khiter_t k = kh_get(clientStatuses, clientStatuses, watcher->fd); // Find it in the hash
	if (k != kh_end(clientStatuses)) { // Was it in the hash? It should have been...
		free(kh_val(clientStatuses, k)); // Free the struct
		kh_del(clientStatuses, clientStatuses, k); // Remove it from the hash
	}
	free(watcher); // Free the watcher (this is last because the fd is used above, after ev_io_stop)
}

// This is called when the headers are received so we can look for a message waiting for
// this person, or leave them connected until one comes, or time them out after 50s maybe?
void receivedHeaders(struct clientStatus *thisClient, struct ev_io *watcher) {
	printf ("Connected by >%s<\r\n", thisClient->clientId);

	// TODO check to see if there's a message queued for this person
	char *output = "HTTP/1.1 200 OK\r\nContent-Length: 42\r\nConnection: close\r\n\r\nabcdefghijklmnopqrstuvwxyz1234567890abcdef";
	write(watcher->fd, output, strlen(output));
}

/* Read client message */
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	byte buffer[BUFFER_SIZE];
	size_t read;

	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	// Look up this socket's client status from the hash
	khiter_t k = kh_get(clientStatuses, clientStatuses, watcher->fd);
	if (k == kh_end(clientStatuses)) {
		puts ("Couldn't find client status in hash!");
		// TODO shut down this connection
		return;
	}
	struct clientStatus *thisClient = kh_val(clientStatuses, k);

	// Receive message from client socket
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);
	
	if (read < 0) {
		puts ("read error");
		// TODO shut down this connection
		return;
	}
	if (read == 0) {
		// Stop and free watcher if client socket is closing
		closeConnection(watcher); // TODO is the socket close in this function necessary since the other side closed it anyway?
		puts("peer closing");
		return;
	}
	// Go through the bytes read
	for (int i=0; i<read; i++) {
		// Are we reading (and ignoring) the rest of the headers?
		if (thisClient->readStatus == 500) { // looking for the second \n to signify the end of headers
			if (buffer[i]=='\n') {
				thisClient->readStatus = 1000; // Now we are ready to respond
				receivedHeaders(thisClient, watcher); // Now we can respond
				closeConnection(watcher);
				return; // No point processing the rest of the stuff from the client: TODO what about skipping everything after the first line?
			} else {
				// TODO throw error and give up - '\r' not followed by '\n'	
			}
		}
		if (thisClient->readStatus == 400) { // looking for the second \r to signify the end of headers, or the next header
			if (buffer[i]=='\r') {
				thisClient->readStatus = 500; // Waiting for the next '\n'
			} else {
				thisClient->readStatus = 200; // Back to reading another header line
			}
		}
		if (thisClient->readStatus == 300) { // looking for the first \n
			if (buffer[i]=='\n') {
				thisClient->readStatus = 400; // Waiting for the next '\r'
			} else {
				// TODO throw error and give up - '\r' not followed by '\n'	
			}
		}
		if (thisClient->readStatus == 200) { // reading (and ignoring) the rest of the headers
			if (buffer[i]=='\r') {
				thisClient->readStatus = 300; // Waiting for a '\n'
			}
		}
		// Are we reading the first line of the header?
		// Did we just receive the '\r' and are we now waiting for the '\n' ?
		if (thisClient->readStatus == 100) {
			if (buffer[i]=='\n') {
				thisClient->readStatus = 200; // Great, now we're going thru the rest of the headers
			} else {
				// Bugger, it wasn't a \n. Drop the connection
				// TODO shut down the connection
			}
		}
		// Reading the rest of the first header line, waiting for the '\r'
		if (thisClient->readStatus == 20) {
			if (buffer[i]=='\r') {
				thisClient->readStatus = 100; // Now waiting for the '\n'
			}
		}
		// Reading the '.js' after the client id
		if (thisClient->readStatus == 12) {
			if (buffer[i]=='s') {
				thisClient->readStatus=20;
			} else {
				// drop the connection, they might be trying to access the favicon or something annoying like that	
				puts ("Not a .js request!");
				closeConnection(watcher);
				return;
			}
		}
		if (thisClient->readStatus == 11) {
			if (buffer[i]=='j') {
				thisClient->readStatus=12;
			} else {
				// drop the connection, they might be trying to access the favicon or something annoying like that	
				puts ("Not a .js request!");
				closeConnection(watcher);
				return;
			}
		}
		// Reading the client id up to the '.js'
		if (thisClient->readStatus == 10) {
			if (buffer[i]=='.') {
				thisClient->clientId[thisClient->clientIdLen]=0; // Put the null terminator on the end of the client id
				thisClient->readStatus = 11; // now reading the rest of the first header line, waiting for the '\r'
			} else {
				// Record the client id
				if (thisClient->clientIdLen < MAX_CLIENT_ID_LEN) {
					thisClient->clientId[thisClient->clientIdLen] = buffer[i];
					thisClient->clientIdLen++;
				} else {
					// Client id too long
					// TODO shut down the connection and print a warning
				}
			}
		}
		// Are we receiving the first line's "GET /" part?
		if (thisClient->readStatus == 0) {
			if (buffer[i]=='/') {
				thisClient->readStatus = 10; // Reading the client id now
				thisClient->clientIdLen = 0;
			}
		}
		thisClient->bytes++;
	}
}
