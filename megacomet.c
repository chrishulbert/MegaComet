// Worth looking at:
// https://github.com/coolaj86/libev-examples/blob/master/src/unix-echo-server.c
// http://codefundas.blogspot.com/2010/09/create-tcp-echo-server-using-libev.html
// http://en.wikipedia.org/wiki/Berkeley_sockets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <ev.h>

#define PORT_NO 8080
#define BUFFER_SIZE 1024
#define LISTEN_BACKLOG 1024 // The number of pending connections that can be queued up at any one time 

struct ev_io port_watcher;
int sd; // The listening socket file descriptor

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

int main(void) {
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

	if(EV_ERROR & revents)
	{
		perror("got invalid event");
		return;
	}

	// Receive message from client socket
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);

	if(read < 0)
	{
	perror("read error");
	return;
	}

	if(read == 0)
	{
	// Stop and free watchet if client socket is closing
	ev_io_stop(loop,watcher);
	free(watcher);
	perror("peer might closing");
	total_clients --; // Decrement total_clients count
	printf("%d client(s) connected.\n", total_clients);
	return;
	}
	else
	{
	printf("message:%s\n",buffer);
	}

	// Send message bach to the client
	send(watcher->fd, buffer, read, 0);
	bzero(buffer, read);
}
