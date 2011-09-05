// This is the mega comet tester
// It opens lots of client connections to a given target ip

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <ev.h>
#include "../khash.h"
#include "../config.h"

// Constants
#define TEST_CONNS 250000
#define REQ_TEMPLATE "GET /%s.js HTTP/1.1\r\nHost: www.example.com\r\nUser-Agent: Some browser\r\nAccept: text/html\r\n\r\n"

// Useful utilities
typedef unsigned char byte;

// Globals (i know, globals are yuck, but we're going for speed not beauty in this code...)
// struct ev_loop *libEvLoop; // The main libev loop. Global so that we don't have to pass it around everywhere
// ev_io conn[TEST_CONNS];
int conns = 0;
char httpRequest[1000];

int findWorker(char* clientIdStr) {
	khint_t hash = kh_str_hash_func(clientIdStr); // Do a crypto hash on the client
	int worker = hash % WORKERS; // Use the hash value to determine which worker they'll be on
}

// Opens a single socket
void openSocket(int worker, char* clientId, char* serverIp) {
	// Open the socket file descriptor
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Can't create a socket");
		exit(1);
	}

	// Build the address of the server
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(COMET_BASE_PORT_NO + worker);
	inet_pton(AF_INET, serverIp, &addr.sin_addr.s_addr);

	// Connect to the manager
	int connectResult = connect(sock, (struct sockaddr*) &addr, sizeof addr);
	if (connectResult < 0) {
		perror("Could not connect");
		exit(1);
	}

	// Now send an HTTP request header
	snprintf(httpRequest,1000,REQ_TEMPLATE,clientId);
	write(sock, httpRequest, strlen(httpRequest));
}

// Find 64k id's that all point to the specified worker
void runTestsWithPrefix(char* prefix, char* serverIp) {
	printf("Running tests against prefix %s\n", prefix);
	int clientNum=0;
	char clientIdStr[20];
	for (clientNum=0; clientNum < TEST_CONNS; clientNum++) {
		snprintf(clientIdStr, 20, "%s%d", prefix, clientNum);
		int worker = findWorker(clientIdStr);
		openSocket(worker, clientIdStr, serverIp);
		if (!(clientNum%1000)) {
			printf("%s\n", clientIdStr);
		}
	}
	printf ("Opened all\n");
}

int main(int argc, char **args) {
	if (argc<3) {
		puts("MegaComet Tester");
		puts("Usage: megatest X Y");
		puts("Where X is the prefix for the client ids: (eg A-D)");
		puts("And Y is the IP address of the comet server: (eg 1.2.3.4)");
		printf("Creates %d connections\n", TEST_CONNS);
		return 1;
	}
	runTestsWithPrefix(args[1], args[2]);
	
	printf("Press enter to quit");
	char temp[10];
	fgets(temp,10,stdin);

	return 0;
}
