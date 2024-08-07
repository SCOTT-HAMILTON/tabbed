#include "zmq_server.h"
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

int zmq_server_bind(ZmqServer *server) {
  server->context = zmq_ctx_new ();
  server->socket = zmq_socket (server->context, ZMQ_REQ);
  zmq_setsockopt (server->socket, ZMQ_IDENTITY, "tabbed", 6);
  int rc = zmq_bind (server->socket, "tcp://0.0.0.0:*");
  if (rc != 0) {
    dprintf (server->log_file, "[error-tabbed] zmq_bind failed: %s\n",
        zmq_strerror (errno));
    return -1;
  } else {
    int timeout = 0;
    rc = zmq_setsockopt (server->socket,
        ZMQ_LINGER, &timeout, sizeof (timeout));
    if (rc != 0) {
      dprintf (server->log_file,
          "[error-tabbed] zmq_setsockopt(LINGER, 0) failed: %s\n",
          zmq_strerror (errno));
      return -1;
    } else {
      int port = zmq_server_get_port(server);
      if (port == -1) {
        return -1;
      } else {
        server->port = (unsigned long)port;
        return 0;
      }
    }
  }
}

int zmq_server_get_port(ZmqServer *server) {
  char bind_address[MAX_SOCKET_STRING];
  size_t addr_length = sizeof (bind_address);

  int rc = zmq_getsockopt (server->socket,
      ZMQ_LAST_ENDPOINT, bind_address, &addr_length);
  if (rc != 0) {
    dprintf (server->log_file,
        "[error-tabbed] zmq_getsockopt(zmq_last_endpoint) failed: %s\n",
        zmq_strerror (errno));
    return -1;
  } else {
    char* portStr = strrchr (bind_address, ':') + 1;
    int result = strtoul(portStr, NULL, 10);
    if (result == ULONG_MAX) {
      return -1;
    } else {
      return result;
    }
  }
}

int zmq_server_recv_nb(ZmqServer *server, char* buf, int bufsize) {
  if (buf == NULL) {
    dprintf (server->log_file,
        "[error-tabbed] zmq_server_recv_nb: invalid buffer received.\n");
    return -1;
  }
  zmq_msg_t msg;
  if (zmq_msg_init(&msg) != 0) {
    dprintf (server->log_file,
        "[error-tabbed] zmq_msg_init failed: %s\n",
        zmq_strerror (errno));
    return -1;
  }
  int nbytes = zmq_msg_recv (&msg, server->socket, ZMQ_DONTWAIT);
  if (nbytes == -1) {
    if (errno == EFSM) {
      /* dprintf (server->log_file, */
          /* "[log-tabbed] zmq_msg_recv skipping, can't receive in current state...\n"); */
      return -1;
    }
    else if (errno != EAGAIN) {
      dprintf (server->log_file,
          "[error-tabbed] zmq_msg_recv failed: zmq_errno=%d, %s\n",
          errno, zmq_strerror (errno));
    }

    return -1;
  }
  size_t msgsize = zmq_msg_size(&msg);
  int max = bufsize-2;
  if (msgsize > max) {
    dprintf (server->log_file,
        "[error-tabbed] received message too big, exceeding buffer"
        ": %lu bytes, max is %d\n", msgsize, max);
    return -1;
  }
  memcpy (buf, zmq_msg_data (&msg), msgsize);
  buf[msgsize] = '\0';
  /* dprintf (server->log_file, "[error-tabbed] msg size: %lu\n", msgsize); */
  return 0;
}

void zmq_server_send(ZmqServer* server, const char* message, size_t size) {
  zmq_msg_t msg;
  int rc = zmq_msg_init_size (&msg, (size + 2));

  if (rc != 0) {
    dprintf (server->log_file, "[error-tabbed] zmq_bind failed: %s\n",
        zmq_strerror (errno));
    return;
  }
  memcpy (zmq_msg_data (&msg), message, size);
  rc = zmq_msg_send (&msg, server->socket, ZMQ_DONTWAIT);
  if (rc != size) {
    if (errno == EFSM) {
      dprintf (server->log_file, "[log-tabbed] zmq_msg_send ignored msg: '%s', socket not ready...\n", message);
      return;
    }
    if (errno != EAGAIN) {
      dprintf (server->log_file,
          "[error-tabbed] zmq_msg_send failed: "
          "%d bytes send, expected it to be %lu: %s\n.",
          rc, size, zmq_strerror (errno));
    }
  }
  dprintf (server->log_file,
      "[log-tabbed] sent msg `%s`.\n", message);
}

void zmq_server_destroy(ZmqServer *server) {
  zmq_close (server->socket);
  zmq_ctx_destroy (server->context);
}

