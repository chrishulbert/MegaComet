// Worth looking at:
// https://github.com/coolaj86/libev-examples/blob/master/src/unix-echo-server.c
// http://codefundas.blogspot.com/2010/09/create-tcp-echo-server-using-libev.html
// http://en.wikipedia.org/wiki/Berkeley_sockets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <ev.h>
#include "khash.h"

#define PORT_NO 8080 // Which port are we listening on
#define BUFFER_SIZE 1024 // The size of the read buffer
#define LISTEN_BACKLOG 1024 // The number of pending connections that can be queued up at any one time 
#define FIRST_LINE_SIZE 60 // The length of the first line allowed (the GET line) - each byte here equals a meg when multiplied by 1m!

// Useful utilities
typedef unsigned char byte;

// Globals (yuck!)
struct ev_io port_watcher;
int sd; // The listening socket file descriptor

// For the status of each connection, we have a hash that goes from the socket file descriptor to the below struct:
struct clientStatus {
	char firstLine[60]; // Eg GET /237f0c36-d661-43d2-b944-13d708c17d36 HTTP/1.1
	int endFirstLineStatus; // 0=nothing, 1=found '\r', 2=found '\n'
	int endHeadersStatus; // 0=nothing, 1=found '\r', 2='\n', 3=2nd '\r', 4=2nd '\n'
};
KHASH_MAP_INIT_INT(clientStatuses, clientStatus*); // Creates the macros for dealing with this hash
khash_t(clientStatuses) *clientStatuses; = kh_init(clientStatuses); // Malloc the hash

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

void openSocket(void) {
	// Open the socket file descriptor
	sd = socket(PF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		perror("socket error");
		exit(-1);
	}

	// Bind the socket to the address
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_NO);
	addr.sin_addr.s_addr = INADDR_ANY;
	int bindResult = bind(sd, (struct sockaddr*) &addr, sizeof(addr));
	if (bindResult < 0) {
		perror("bind error");
		exit(-1);
	}

	// Start listing on the socket
	int listenResult = listen(sd, LISTEN_BACKLOG);
	if (listenResult < 0) {
		perror("listen error");
		exit(-1);
	}

	puts("Socket opened");
}

// The main libev loop
void run() {
	// use the default event loop unless you have special needs
	struct ev_loop *loop = ev_default_loop(0);

	// Initialize and start a watcher to accepts client requests
	ev_io_init(&port_watcher, accept_cb, sd, EV_READ);
	ev_io_start(loop, &port_watcher);

	puts("Libev initialised, starting...");

	// Start infinite loop
	ev_loop(loop, 0);
}

// Initialise the hash tables that are needed
void initHashes() {
	clientStatuses = kh_init(clientStatuses); // Malloc the hash

	// int ret, is_missing;
	// khiter_t k;
	// khash_t(socketBytes) *h = kh_init(socketBytes); // Malloc the hash
	// k = kh_put(socketBytes, h, 5, &ret); // Insert 5 to the table
	// if (!ret) kh_del(socketBytes, h, k); // Delete it if it was there before we put it in
	// kh_value(h, k) = 10; // Change it to 10
	// k = kh_get(socketBytes, h, 10); // Get the value at 10
	// is_missing = (k == kh_end(h)); // Is there anything there?
	// k = kh_get(socketBytes, h, 5); // Get the value at 5
	// kh_del(socketBytes, h, k); // Delete the value at 5
	// for (k = kh_begin(h); k != kh_end(h); ++k) // Iterate somehow 
	// 	if (kh_exist(h, k)) kh_value(h, k) = 1;
}

// All the setup stuff goes here
void setup() {
	initHashes();
	openSocket();
}

// All the shutdown stuff goes here
void shutDown() {
	kh_destroy(clientStatuses, clientStatuses); // Free it all
}

// Everyone's favourite function!
int main() {
	setup();
	run();
	shutDown();
	return 0;
}

/* Accept client requests */
void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	struct ev_io *w_client = (struct ev_io*) malloc (sizeof(struct ev_io));

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
	struct clientStatus *newStatus = calloc(1, sizeof(clientStatus));
	int ret;
	kh_put(clientStatuses, clientStatuses, newStatus, &ret)

	// Initialize and start watcher to read client requests
	ev_io_init(w_client, read_cb, client_sd, EV_READ);
	ev_io_start(loop, w_client);
}

/* Read client message */
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	byte buffer[BUFFER_SIZE];
	ssize_t read;

	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	// Look up this socket's client status from the hash
	khiter_t k = kh_get(clientStatuses, clientStatuses, watcher->fd);
	if (k == kh_end(h)) {
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
	if(read == 0) {
		// Stop and free watcher if client socket is closing
		ev_io_stop(loop,watcher);
		free(watcher);
		puts("peer might closing");
		return;
	}
	// Go through the bytes read
	for (int i=0; i<read; i++) {
		// Did we just receive the '\r' and are we now waiting for the '\n' ?
		if (thisClient->endFirstLineStatus == 1) {
			if (buffer[i]=='\n') {
				thisClient->endFirstLineStatus = 2; // Great, now we're going thru the rest of the
			} else {
				// Bugger, it wasn't a \n. Drop the connection
				// TODO shut down the connection
			}
		}
		// Are we still receiving the first line?
		if (thisClient->endFirstLineStatus == 0) {
			if (buffer[i]=='\r') {
				thisClient->endFirstLineStatus = 1; // Move to waiting for the '\n'
			} else {
				// Received another byte of the first line
				// Have we still got space to store it?
				if (thisClient->bytes < FIRST_LINE_SIZE) {
					thisClient->firstLine[thisClient->bytes] = buffer[i];
				} else {
					// The first line is too long, so shut down the connection rather than have a buffer overflow
					// TODO shut down this connection
				}
			}
		}
		
		firstLine[60]; // Eg GET /237f0c36-d661-43d2-b944-13d708c17d36 HTTP/1.1
	int endFirstLineStatus; // 0=nothing, 1=found '\r', 2=found '\n'
	int endHeadersStatus
		buffer[i]

		thisClient->bytes++;
	}

	// Send message bach to the client
	send(watcher->fd, buffer, read, 0);
	bzero(buffer, read);
}
