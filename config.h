// Common configuration for megacomet worker and manager

#ifndef _CONFIG_H
#define _CONFIG_H

#define MANAGER_PORT_NO 9000 // The port we are to listen for the workers
#define LISTEN_BACKLOG 1024 // The number of pending connections that can be queued up at any one time 
#define WORKERS 8 // The number of workers
#define MAX_MANAGER_CONNS 16 // We need to cater for N connections. Usually 8 workers + 1 (or more) app connections
#define MAX_CLIENT_ID_LEN 128 // Length of the client id's
#define MAX_MESSAGE_LEN 1024 // Length of the message
#define BUFFER_SIZE 2048 // Size of the chunks we read incoming commands in. Should be big enough for a full command

#define COMET_BASE_PORT_NO 8000 // Which port range are we listening on for clients
//#define FIRST_LINE_SIZE 60 // The length of the first line allowed (the GET line) - each byte here equals a meg when multiplied by 1m!
#define HTTP_TEMPLATE "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s" // The http response
#define HTTP_OVERHEAD 80 // The size of the above line, plus a few bytes
#define HTTP_RESPONSE_SIZE (MAX_MESSAGE_LEN + HTTP_OVERHEAD) // Size of the http response buffer

#endif

