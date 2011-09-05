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
#include "klist.h"
#include "config.h"

// Useful utilities
typedef unsigned char byte;

// Globals
int workerNo; // Which worker number this is 
int cometSd, managerSd; // The listening socket file descriptor
struct ev_loop *libEvLoop; // The main libev loop. Global so that we don't have to pass it around everywhere, slowly pushing and popping it to the stack
struct ev_io cometPortWatcher; // The watcher for incoming comet conns
struct ev_io managerPortWatcher; // The watcher for incoming manager commands

// Stuff for the manager connection
byte commandClientId[MAX_CLIENT_ID_LEN+1];
int commandClientIdLen;
byte commandMessage[MAX_MESSAGE_LEN+1];
int commandMessageLen;
int commandStatus=0; // 0 = nothing, waiting
// 200=read a '2', reading the client id
// 201=read the client, reading the message

// For creating the http response message
char httpResponse[HTTP_RESPONSE_SIZE];

// For the status of each connection, we have the below struct, which extends the io watcher
typedef struct clientStatus {
	ev_io io; // The IO watcher. This is first so that when the callback is called, we can cast it to a clientStatus.
	int readStatus; // 0=nothing, waiting for '/'
		// First line: 10=found '/', reading client id
		// 20=found '.', reading the rest of the first line
		// First line: 100=found '\r', 200=found '\n' 
		// Reading headers: 200, 300=found '\r', 400='\n', 500=2nd '\r', 1000=found 2nd '\n'
		// Ready to respond: 1000
	int clientIdLen; // Length of the client id
	char clientId[MAX_CLIENT_ID_LEN+1]; // Eg will be 'myClientId' for: GET /myClientId.js?c=cachekiller HTTP/1.1
} clientStatus;

// The memory pool of client statuses
#define __nop_free(x)
KMEMPOOL_INIT(csPool, clientStatus, __nop_free); // Set up the macros for the client status memory pool
kmempool_t(csPool) *csPool; // The memory pool

// The hash of client id's to client statuses
KHASH_MAP_INIT_STR(clientStatuses, clientStatus*); // Creates the macros for dealing with this hash
khash_t(clientStatuses) *clientStatuses; // The hash table

// The queue of messages waiting to be collected
// TODO every few minutes, iterate through this list to clear old ones out
// This is a hash from client id to list
KLIST_INIT(messages, char*, __nop_free); // The message list for a single client type
KHASH_MAP_INIT_STR(queue, klist_t(messages)*); // The queue hash table type
khash_t(queue) *queue; // The queue hash table

void newConnectionCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void readCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void managerCallback(struct ev_loop *loop, struct ev_io *watcher, int revents);

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
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(COMET_BASE_PORT_NO+workerNo);
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

	// puts("Socket opened");
}

// Open the connection to the manager
void openManagerSocket(void) {
	// Open the socket file descriptor
	managerSd = socket(PF_INET, SOCK_STREAM, 0);
	if (managerSd < 0) {
		perror("manager socket error");
		exit(1);
	}

	// Build the address of the manager
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MANAGER_PORT_NO);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

	// Connect to the manager
	// puts ("Connecting to manager...");
	int connectResult = connect(managerSd, (struct sockaddr*) &addr, sizeof addr);
	if (connectResult < 0) {
		perror("Could not connect to manager. Start the manager first!");
		exit(1);
	}

	// Now tell the manager which worker i am
	byte msg[2];
	msg[0]=1;
	msg[1]=workerNo;
	write(managerSd, msg, 2);

	// puts("Manager connected");
}

// The main libev loop
void run() {
	// use the default event loop unless you have special needs
	libEvLoop = ev_default_loop(0);

	// The watcher for incoming comet connections
	ev_io_init(&cometPortWatcher, newConnectionCallback, cometSd, EV_READ);
	ev_io_start(libEvLoop, &cometPortWatcher);

	// The watcher for manager commands on the already-open socket
	ev_io_init(&managerPortWatcher, managerCallback, managerSd, EV_READ);
	ev_io_start(libEvLoop, &managerPortWatcher);

	// puts("Ready");

	// Start infinite loop
	ev_loop(libEvLoop, 0);
}

// Initialise the hash tables that are needed
void initHashes() {
	csPool = kmp_init(csPool);
	clientStatuses = kh_init(clientStatuses); // Malloc the hash
	queue = kh_init(queue);
}

// All the setup stuff goes here
void setup() {
	initHashes();
	openCometSocket();
	openManagerSocket();
}

// All the shutdown stuff goes here. Is it really worth bothering to clean up memory just prior to exit?
void shutDown() {
	close(cometSd);
	close(managerSd);
	kmp_destroy(csPool, csPool); // Free the pooled client statuses
	kh_destroy(clientStatuses, clientStatuses); // Free it all
	kh_destroy(queue, queue); // Todo: this probably wont destroy the lists in each queue hash value
	// Todo clean up the libev stuff
}

// Everyone's favourite function!
int main(int argc, char **args) {
	// Suss out the command line
	if (argc<2) {
		puts("MegaComet worker");
		puts("This should be started by the MegaStart, not called directly");
		return 1;
	}
	workerNo = atoi(args[1]);
	
	setup();
	run();
	shutDown();
	return 0;
}

// Accept client requests
void newConnectionCallback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	if (EV_ERROR & revents) {
		puts("got invalid event");
		return;
	}

	// Accept client request
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	int clientSd = accept(watcher->fd, (struct sockaddr *)&clientAddr, &clientAddrLen);

	if (clientSd < 0) {
		puts("accept error");
		return;
	}

	// Create a client status by getting it from the memory pool
	clientStatus *newStatus = kmp_alloc(csPool, csPool);
	memset(newStatus, 0, sizeof(clientStatus));

	// Initialize and start watcher to read client requests
	ev_io_init(&newStatus->io, readCallback, clientSd, EV_READ);
	ev_io_start(loop, &newStatus->io);
}

// Close a connection and free the memory associated and skip removing from hash, only for use when a connection
// arrives and already was a message waiting for it
void closeConnectionSkipHash(ev_io *watcher) {
	ev_io_stop(libEvLoop, watcher); // Tell libev to stop following it
	close(watcher->fd); // Close the socket
	kmp_free(csPool, csPool, (clientStatus*)watcher); // Free the clientstatus/watcher (this is last because the fd is used above, after ev_io_stop)
}

// Close a connection and free the memory associated and remove from hash
void closeConnection(ev_io *watcher) {
	ev_io_stop(libEvLoop, watcher); // Tell libev to stop following it
	close(watcher->fd); // Close the socket

	// Remove the client status from the hash if it's a waiting connection
	if (((clientStatus*)watcher)->readStatus==1000) { // Only ones waiting a message (1000) are in the hash
		khiter_t k = kh_get(clientStatuses, clientStatuses, ((clientStatus*)watcher)->clientId); // Find it in the hash
		if (k != kh_end(clientStatuses)) { // Was it in the hash?
			kh_del(clientStatuses, clientStatuses, k); // Remove it from the hash
		}
	}

	kmp_free(csPool, csPool, (clientStatus*)watcher); // Free the clientstatus/watcher (this is last because the fd is used above, after ev_io_stop)
}

// Called when the manager sends a complete message
void messageArrivedFromManager() {
	printf ("Message arrived: >%s< for >%s<\r\n", commandMessage, commandClientId);

	// See if the client is connected, if so immediately forward
	khiter_t k = kh_get(clientStatuses, clientStatuses, (char*)commandClientId); // Find it in the hash
	if (k != kh_end(clientStatuses)) { // Was it in the hash?
		clientStatus* status = kh_value(clientStatuses, k); // Grab the clientStatus from the hash
		snprintf(httpResponse, HTTP_RESPONSE_SIZE, HTTP_TEMPLATE, commandMessageLen, commandMessage); // Compose the response message
		write(status->io.fd, httpResponse, strlen(httpResponse)); // Send it
		closeConnection((ev_io*)status); // Close the conn
		return;
	}

	// If not, add to a queue
	khiter_t q = kh_get(queue, queue, (char*)commandClientId); // See if this client is already in the queue
	if (q == kh_end(queue)) {
		printf("Creating queue for %s\r\n", commandClientId);
		// This client needs to be added to the queue
		// First make a new list
		klist_t(messages) *newMessageList = kl_init(messages);
		*kl_pushp(messages, newMessageList) = strdup((char*)commandMessage); // Add the message to the list
		// Now make a new hash entry pointing to this new list
		int ret;
		q = kh_put(queue, queue, strdup((char*)commandClientId), &ret);
		kh_value(queue, q) = newMessageList;
	} else {
		printf("Adding to the queue for %s\r\n", commandClientId);
		// This client is in the queue already eg it has a hash entry
		// Pushp puts this message at the end of the queue, so that shift will grab the oldest first (like a FIFO)
		*kl_pushp(messages, kh_value(queue, q)) = strdup((char*)commandMessage);
	}

	// Now do a printout of the hash list
	for (khiter_t qi = kh_begin(queue); qi < kh_end(queue); qi++) {
		if (kh_exist(queue, qi)) {
			printf("Queue for %s\n", kh_key(queue,qi));
			klist_t(messages) *list = kh_value(queue, qi);
			kliter_t(messages) *li;
			for (li = kl_begin(list); li != kl_end(list); li = kl_next(li))
				printf("%s\n", kl_val(li));
			printf("----\n");
		}
	}
}

// This gets called when there's an incoming command from the manager
void managerCallback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	byte buffer[BUFFER_SIZE];
	size_t read;

	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	// Receive message from client socket
	read = recv(managerSd, buffer, BUFFER_SIZE, 0);
	
	if (read < 0) {
		puts ("manager read error");
		// TODO reconnect to the manager?
		// Or should i exit, and let a monitoring script look after re-launching?
		return;
	}
	if (read == 0) {
		puts ("manager connection closing");
		// TODO reconnect to the manager?
		// Or should i exit, and let a monitoring script look after re-launching?
		// I'm thinking i should exit because the manager has probably died
		exit(1);
		return;
	}
	// Go through the bytes read and parse what the client is sending us
	for (int i=0; i<read; i++) {
		if (commandStatus==0) {
			if (buffer[i]==2) { // Start of the mgr sending a message
				commandStatus = 200;
				commandClientIdLen = 0;
				commandMessageLen = 0;
				continue;				
			}
		}
		if (commandStatus==200) { // We are waiting for the mgr to send a client id
			if (buffer[i]==0) {
				commandClientId[commandClientIdLen] = 0; // Add the null terminator
				commandStatus=201; // Now wait for the message	
				continue;
			} else {
				if (commandClientIdLen < MAX_CLIENT_ID_LEN) {
					commandClientId[commandClientIdLen] = buffer[i];
					commandClientIdLen ++;
					continue;
				} else {
					// Buffer overrun on the client id, so put the error
					puts("Buffer overrun on the client id from the mgr");
					commandStatus=0;
					continue;
				}
			}
		}
		if (commandStatus==201) { // We are waiting for the app sending a message
			if (buffer[i]==0) {
				commandMessage[commandMessageLen] = 0; // Add the null terminator
				messageArrivedFromManager(); // Send the message to the correct client
				commandStatus=0; // Now wait for the next command	
				continue;
			} else {
				if (commandMessageLen < MAX_MESSAGE_LEN) {
					commandMessage[commandMessageLen] = buffer[i];
					commandMessageLen ++;
					continue;
				} else {
					// Buffer overrun on the message, so put the error and kill this connection todo
					puts("Buffer overrun on the message from the mgr");
					commandStatus=0;
					continue;
				}
			}
		}
		// If it got to the end of the loop here, then the manager has sent a malformed message so lets reset the parser
		commandStatus = 0;
	} // end of the for loop
}

// This is called when the headers are received so we can look for a message waiting for
// this person, or leave them connected until one comes, or time them out after 50s maybe?
void receivedHeaders(clientStatus *thisClient) {
	// printf ("Connected by >%s<\r\n", thisClient->clientId);

	// Check to see if there's a message queued for this person
	// if so, send it and drop the connection
	khiter_t q = kh_get(queue, queue, (char*)thisClient->clientId);
	if (q != kh_end(queue)) {
		char *queuedMessage;
		kl_shift(messages, kh_value(queue,q), &queuedMessage);
		// Now send the message to the person and close
		snprintf(httpResponse, HTTP_RESPONSE_SIZE, HTTP_TEMPLATE, (int)strlen(queuedMessage), queuedMessage); // Compose the response message
		free(queuedMessage);
		write(thisClient->io.fd, httpResponse, strlen(httpResponse)); // Send it
		closeConnectionSkipHash((ev_io*)thisClient);
		// If that was the last one, free the list and remove it from the hash
		if (!kh_value(queue, q)->head->next) {
			kl_destroy(messages, kh_value(queue, q)); // Free the list
			free((void*)kh_key(queue, q)); // Free the key (the client id)
			kh_del(queue, queue, q); // Remove this client id from the hash
		}
	} else {
		// If there's no message, then add their client id to the hash for later
		int ret;
		khiter_t k = kh_put(clientStatuses, clientStatuses, thisClient->clientId, &ret);
		kh_value(clientStatuses, k) = thisClient;
	}

}

/* Read client message */
void readCallback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	if (EV_ERROR & revents) {
		puts ("got invalid event");
		return;
	}

	struct clientStatus *thisClient = (clientStatus*)watcher;

	// Receive message from client socket
	byte buffer[BUFFER_SIZE];
	size_t read;
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);
	
	if (read < 0) {
		puts ("read error");
		// TODO shut down this connection
		return;
	}
	if (read == 0) {
		// Stop and free watcher if client socket is closing
		closeConnection(watcher); // TODO is the socket close in this function necessary since the other side closed it anyway?
		// puts("peer closing");
		return;
	}
	// Go through the bytes read
	for (int i=0; i<read; i++) {
		// Are we reading (and ignoring) the rest of the headers?
		if (thisClient->readStatus == 500) { // looking for the second \n to signify the end of headers
			if (buffer[i]=='\n') {
				thisClient->readStatus = 1000; // Now we are ready to respond
				receivedHeaders(thisClient); // Now we can respond
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
				// puts ("Not a .js request!");
				closeConnection(watcher);
				return;
			}
		}
		if (thisClient->readStatus == 11) {
			if (buffer[i]=='j') {
				thisClient->readStatus=12;
			} else {
				// drop the connection, they might be trying to access the favicon or something annoying like that	
				// puts ("Not a .js request!");
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
	}
}
