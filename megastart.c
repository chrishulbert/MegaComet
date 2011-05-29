// Megastart
// This starts the megacomet manager and workers
// The idea is that you'd have a cron job that would run this every minute, or every 10s if you're keen
// That way this can re-start anything that has crashed
// Which, of course, it won't (hopefully!) :)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "config.h"

// This tests to see if a port is listening. This is a good way to test if the megacomet manager/workers are running.
int isPortFree(int port) {
	// Open the socket file descriptor
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return 0; // Not free
	}

	// Bind the socket to the address
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	int bindResult = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
	if (bindResult < 0) {
		close(sock);
		return 0; // Not Free
	}
	close(sock);
	return 1; // Free
}

// Daemonise the process by forking it
void daemonise() {
	puts ("Running in the background");

	int i=fork();
	if (i<0) exit(1); /* fork error */
	if (i>0) exit(0); /* parent exits */
 
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */	

	// Close out the standard file descriptors
	close(STDIN_FILENO);  
	close(STDOUT_FILENO);  
	close(STDERR_FILENO);
}

// Run one iteration of the daemon loop
void daemonIter() {
	if (isPortFree(MANAGER_PORT_NO)) {
		system("killall megacomet"); // Kill all the workers if the manager died
		system("./megamanager start &");
		sleep(MANAGER_START_DELAY);
	}
	for (int worker=0; worker<WORKERS; worker++) {
		if (isPortFree(COMET_BASE_PORT_NO + worker)) {
			char cmd[20];
			snprintf(cmd, 20, "./megacomet %d &", worker);
			system(cmd);
		}
	}	
}

// Run the daemon loop forever
void daemonLoop() {
	while (1) {
		daemonIter();
		sleep(DAEMON_LOOP_SECONDS);
	}
}

int main(int argc, char **args) {
	puts("MegaComet Starter");
	// Suss out the command line
	if (argc<2) {
		puts("This should be started by the start script, not called directly");
		return 1;
	}

	// Now daemonise
	daemonise();
	daemonLoop();

	return 0;
}

