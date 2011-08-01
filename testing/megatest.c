// This is the mega comet tester
// It opens 64000 client connections to a given target ip

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
#define TEST_CONNS 64000

// Useful utilities
typedef unsigned char byte;

// Globals (i know, globals are yuck, but we're going for speed not beauty in this code...)
struct ev_loop *libEvLoop; // The main libev loop. Global so that we don't have to pass it around everywhere
ev_io conn[TEST_CONNS];
int conns = 0;

int findWorker(char* clientIdStr) {
	khint_t hash = kh_str_hash_func(clientIdStr); // Do a crypto hash on the client
	int worker = hash % WORKERS; // Use the hash value to determine which worker they'll be on
}

// Find 64k id's that all point to the specified worker
void runTestsAgainstWorker(int worker) {
	printf("Running tests against worker %d\n", worker);
	int clientId=0, clientsForWorker=0;
	char clientIdStr[20];
	while (clientsForWorker < TEST_CONNS) {
		snprintf(clientIdStr,20,"%d",clientId);
		if (findWorker(clientIdStr)==worker) {
			clientsForWorker++;
		}
		clientId++;
	}
	printf("It took up to client id %d to fill this worker\n", clientId);
}

int main(int argc, char **args) {
	if (argc<2) {
		puts("MegaComet Tester");
		puts("Usage: megatest N");
		printf("Where N is the number of the client you wish to test: 0-%d inclusive\n", WORKERS-1);
		return 1;
	}
	runTestsAgainstWorker(atoi(args[1]));
	return 0;
}
