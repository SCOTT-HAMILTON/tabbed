#ifndef TABBED_ZMQ_SERVER_H
#define TABBED_ZMQ_SERVER_H

#include <zmq.h>

#define MAX_SOCKET_STRING 256

typedef struct {
	int log_file;
	void* context;
	void* socket;
	int port;
} ZmqServer;

// -1 on error, 0 on success
int zmq_server_bind(ZmqServer *server);
// -1 on error, the port the socket is bounded to otherwise
int zmq_server_get_port(ZmqServer *server);

// Non blocking recv
// -1 on error, 0 on success
int zmq_server_recv_nb(ZmqServer *server, char* buf, int bufsize);

void zmq_server_destroy(ZmqServer *server);

#endif // TABBED_ZMQ_SERVER_H
