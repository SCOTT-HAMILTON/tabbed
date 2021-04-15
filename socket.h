#ifndef TABBED_SOCKET_H
#define TABBED_SOCKET_H

#include "queue.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#define BUFFER_SIZE 12
// Arbitrary length of statically allocated
// file paths
#define SOCKET_FILEPATH_SIZE 256
#define SOCKET_TMP_LOCK_FORMAT "tabbed-lock-XXXXXX"

struct msg {
  char content[256];
  queue_handle qh;
};

typedef struct {
  //// Internal Fields
  // socket that listens to connections.
  // Once a connection is accepted, the newly
  // created filedescriptor is used by poll and stored
  // in poll_fds.
  int listening_socket_fd;
  // pollfd array, should be of size 1,
  // required because poll only accepts pollfd array
  // and not a single pollfd
  struct pollfd poll_fds[1];
  int open_fds_count;
  // The port found by bind to be communicated to the client
  uint16_t socket_port;
  // flag to be enabled when accept() receives a new connection
  int client_accepted;
  // File descriptor to a lock file that will be filled in once the socket is
  // binded. The port of the listening socket is written to it. The client is
  // meanwhile waiting for this lock file to be filled in before reading the
  // port to connect to in it.
  int lock_port_fd;

  // first empty pollin received time
  int64_t first_empty_pollin_received_time;
  // number of consecutive empty pollin received
  int64_t empty_pollin_received_count;

  // message queue to send
  struct msg *messages_queue;

  int log_file;
} SocketListener;

int setup_nonblocking_listener(SocketListener *socket_listener);

int loop_listen_nonblocking(SocketListener *socket_listener, char *message,
                            size_t size, unsigned long timeout);
void loop_send(SocketListener *socket_listener);

// Internal, shouldn't be called
void cleanup_socket(SocketListener *socket_listener);

int socket_send(SocketListener *socket_listener, char *msg, size_t msg_size);

// Utility function
int make_socket_listener(SocketListener *socket_listener,
                         unsigned long UNIQUE_ID);

// Generate random "unique" ID
// (it's not actually unique, TODO: make it unique)
unsigned long make_unique_id();

// Wait for socket port lock to be filled in
// delay is the pause duration in milliseconds
// between each iteration
int wait_for_socket_port_lock(SocketListener *socket_listener, int delay);

int64_t get_posix_time();

#endif // TABBED_SOCKET_H
