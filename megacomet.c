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

struct ev_io port_watcher;
int sd; // The listening socket file descriptor

// Init the various hash tables
KHASH_MAP_INIT_INT(socketBytes, int); // For testing, this is for counting the bytes per socket

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
void libevLoop(void) {
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
	int ret, is_missing;
	khiter_t k;
	khash_t(socketBytes) *h = kh_init(socketBytes); // Malloc the hash
	k = kh_put(socketBytes, h, 5, &ret); // Insert 5 to the table
	if (!ret) kh_del(socketBytes, h, k); // Delete it if it was there before we put it in
	kh_value(h, k) = 10; // Change it to 10
	k = kh_get(socketBytes, h, 10); // Get the value at 10
	is_missing = (k == kh_end(h)); // Is there anything there?
	k = kh_get(socketBytes, h, 5); // Get the value at 5
	kh_del(socketBytes, h, k); // Delete the value at 5
	for (k = kh_begin(h); k != kh_end(h); ++k) // Iterate somehow 
		if (kh_exist(h, k)) kh_value(h, k) = 1;
	kh_destroy(socketBytes, h); // Free it all
}

int main(void) {
	initHashes();
	openSocket();
	libevLoop();
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

	// Initialize and start watcher to read client requests
	ev_io_init(w_client, read_cb, client_sd, EV_READ);
	ev_io_start(loop, w_client);
}

/* Read client message */
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	char buffer[BUFFER_SIZE];
	ssize_t read;

	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	// Receive message from client socket
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);

	if (read < 0) {
		puts ("read error");
		return;
	}

	if(read == 0) {
		// Stop and free watcher if client socket is closing
		ev_io_stop(loop,watcher);
		free(watcher);
		puts("peer might closing");
		return;
	}
	else {
		printf("message:%s\n",buffer);
	}

	// Send message bach to the client
	send(watcher->fd, buffer, read, 0);
	bzero(buffer, read);
}
