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
#define TEST_CONNS 250000

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
void runTestsWithPrefix(char* prefix) {
	printf("Running tests against prefix %s\n", prefix);
	int clientId=0;
	char clientIdStr[20];
	int workerCounts[WORKERS]={};
	while (clientId < TEST_CONNS) {
		snprintf(clientIdStr, 20, "%s%d", prefix, clientId);
		int worker = findWorker(clientIdStr);
		workerCounts[worker]++;
		clientId++;
	}
	for (int i=0;i<WORKERS;i++) {
		printf("I'm aiming %d connections at worker #%d\n", workerCounts[i], i);
	}
}

int main(int argc, char **args) {
	if (argc<2) {
		puts("MegaComet Tester");
		puts("Usage: megatest X");
		puts("Where X is the prefix for the client ids: (eg A-D)");
		printf("Creates %d connections\n", TEST_CONNS);
		return 1;
	}
	runTestsWithPrefix(args[1]);
	return 0;
}
