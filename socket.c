#include "socket.h"

#include <arpa/inet.h>
#include <bsd/string.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int
setup_nonblocking_listener(SocketListener* socket_listener)
{
    struct sockaddr_in addr;
    int ret;
	
	socket_listener->listening_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_listener->listening_socket_fd == -1) {
		dprintf(socket_listener->log_file, "[log-tabbed] setup_nonblocking_listener can't open socket, socket() == -1\n");
		return -1;
    }
    /*
     * For portability clear the whole structure, since some
     * implementations have additional (nonstandard) fields in
     * the structure.
     */
    memset(&addr, 0, sizeof(struct sockaddr_in));

    /* Bind socket to socket name. */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = 0; /* Let bind assign an ephemeral port */
    ret = bind(socket_listener->listening_socket_fd, (const struct sockaddr *) &addr,
               sizeof(struct sockaddr_in));
    if (ret == -1) {
        dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener : socket bind(%u, %s, %u) = -1\n",
				socket_listener->listening_socket_fd, inet_ntoa(addr.sin_addr), htons(addr.sin_port));
        return -1;
    }
	{
		struct sockaddr_in given_addr;
		socklen_t socket_len = sizeof(given_addr);
		if (getsockname(socket_listener->listening_socket_fd,
					(struct sockaddr*)&given_addr,  &socket_len) == -1) {
			dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener : can't get assigned port, getsockname(%u)\n",
					socket_listener->listening_socket_fd);
			return -1;
		} else {
			socket_listener->socket_port = ntohs(given_addr.sin_port);
			/* dprintf(socket_listener->log_file, "[log-tabbed] setup_nonblocking_listener : assigned socket port is `%u`\n", */
			/* 		socket_listener->socket_port); */
			char buf[10];
			if (snprintf(buf, 9, "%u", socket_listener->socket_port) >= 9) {
				// Impossible case ?
				dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener, buffer too little\n");
				return -1;
			}
			if (write(socket_listener->lock_port_fd, buf, 9) == -1) {
				dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener : can't write assigned port to lock file, write(%u, %s) == -1\n",
						socket_listener->lock_port_fd, buf);
				return -1;
			} else {
				// dprintf(socket_listener->log_file, "[log-tabbed] setup_nonblocking_listener : port written to lock\n");
			}
		}
	}
	
    /*
     * Prepare for accepting connections. The backlog size is set
     * to 2. So while one request is being processed other requests
     * can be waiting.
	 * 1 should only be necessary though
     */
    ret = listen(socket_listener->listening_socket_fd, 2);
    if (ret == -1) {
        dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener : listen(%u, 2)\n", socket_listener->listening_socket_fd);
        return -1;
    }

	socket_listener->client_accepted = 0;

    return 0;
}

// To be called continuously with delay between calls
// until 1 is returned.
// Returns 2 if a message is received
// This non-blocking implementation avoids multithreading
int
loop_listen_nonblocking(SocketListener* socket_listener, char* buffer, size_t size, unsigned long timeout) {
	if (!socket_listener->client_accepted) {
		/* Wait for incoming connection.
		 * and accept the new file descriptor into
		 * poll_fds
		 * */
		socket_listener->poll_fds[0].fd = accept(socket_listener->listening_socket_fd, NULL, NULL);
		/* dprintf(socket_listener->log_file, "accepted\n"); */
		switch (socket_listener->poll_fds[0].fd) {
			case EAGAIN:
				// Socket is empty, no queued message to accept, error...
				return -1;
			case -1:
				dprintf(socket_listener->log_file, "[error-tabbed] setup_nonblocking_listener : accept(%u) == -1", socket_listener->listening_socket_fd);
				return -1;
		}
		// Setup the poll after a socket has been accepted
		socket_listener->open_fds_count = 1;
		socket_listener->poll_fds[0].events =
			POLLIN|
			POLLRDNORM|
			POLLRDBAND|
			POLLPRI|
			POLLOUT|
			POLLWRNORM|
			POLLWRBAND|
			POLLERR|
			POLLHUP|
			POLLNVAL;
		socket_listener->client_accepted = 1;
	}
	if (socket_listener->open_fds_count > 0) {
		int ready;
		// Poll one fd during timeout ms
		ready = poll(socket_listener->poll_fds, 1, timeout);
		switch (ready) {
			case -1:
				dprintf(socket_listener->log_file, "[error-tabbed] loop_listen_nonblocking : poll(fd, %lums) = -1\n", timeout);
				return -1;
			case 0:
				// Timed out
				return 0;
		}
		/* dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking : poll ready: %d\n", ready); */

		char buf[256];
		if (socket_listener->poll_fds[0].revents != 0) {
			/* if */ 
			/* 	(!(socket_listener->poll_fds[0].revents & POLLIN || */ 
			/* 	  socket_listener->poll_fds[0].revents & POLLOUT)) */
			/* { */
			/* 		dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking fd=%d; events: %s%s%s%s%s%s%s%s\n", socket_listener->poll_fds[0].fd, */
			/* 			/1* (socket_listener->poll_fds[0].revents & POLLIN)  ? "POLLIN "  : "", *1/ */
			/* 			(socket_listener->poll_fds[0].revents & POLLRDNORM) ? "POLLRDNORM " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLRDBAND) ? "POLLRDBAND " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLPRI) ? "POLLPRI " : "", */
			/* 			/1* (socket_listener->poll_fds[0].revents & POLLOUT) ? "POLLOUT " : "", *1/ */
			/* 			(socket_listener->poll_fds[0].revents & POLLWRNORM) ? "POLLWRNORM " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLWRBAND) ? "POLLWRBAND " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLERR) ? "POLLERR " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLHUP) ? "POLLHUP " : "", */
			/* 			(socket_listener->poll_fds[0].revents & POLLNVAL) ? "POLLNVAL " : ""); */
			/* } */
			if (socket_listener->poll_fds[0].revents & POLLIN) {
				ssize_t s = recv(socket_listener->poll_fds[0].fd, buf, sizeof(buf), 0);
				if (s == -1) {
					// When read error, exit and wait for another message (by recalling the loop)
					dprintf(socket_listener->log_file, "[error-tabbed] loop_non_blocking recv(%u) == -1\n", socket_listener->poll_fds[0].fd);
					return 0;
				}
				if (s > 0) {
					socket_listener->empty_pollin_received_count = 0;
					buffer[0] = '\0';
					strlcpy(buffer, buf, s+1);
					/* fprintf(stderr, "[log-tabbed] received : `%s` => `%s`:`%ld`\n", */
					/* 		buf, buffer, s+1); */
					dprintf(socket_listener->log_file, "[log-tabbed] loop_non_blocking : received %zd bytes: %.*s\n", s, (int) s, buffer);
					return 2;
				} else if (s == 0) {
					if (socket_listener->empty_pollin_received_count == 0) {
						socket_listener->first_empty_pollin_received_time = get_posix_time();
					}
					++socket_listener->empty_pollin_received_count;
					int64_t time = get_posix_time();
					// If we received 10 or more empty POLLIN within 1 seconds
					if (socket_listener->empty_pollin_received_count > 10
							&& time-socket_listener->first_empty_pollin_received_time < 1000) {
						if (close(socket_listener->poll_fds[0].fd) == -1) {
							dprintf(socket_listener->log_file, "[error-tabbed] loop_listen_nonblocking : can't close fd `%u`\n", socket_listener->poll_fds[0].fd);
						} else {
							/* dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking : empty POLLINS, closed fd %d\n", socket_listener->poll_fds[0].fd); */
							socket_listener->open_fds_count--;
							socket_listener->empty_pollin_received_count = 0;
							socket_listener->first_empty_pollin_received_time = 0;
						}
					}
				} else {
					dprintf(socket_listener->log_file, "[error-tabbed] loop_listen_nonblocking : read failed\n");
				}
				/* else { */
				/* 	dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking closing fd %d\n", socket_listener->poll_fds[0].fd); */
				/* 	if (close(socket_listener->poll_fds[0].fd) == -1) { */
				/* 		dprintf(socket_listener->log_file, "[error-tabbed] loop_listen_nonblocking : can't close fd `%u`\n", socket_listener->poll_fds[0].fd); */
				/* 	} */
				/* 	socket_listener->open_fds_count--; */
				/* 	return 0; */
				/* } */
			}
			if (socket_listener->poll_fds[0].revents & POLLOUT) {
				// can send messages
				loop_send(socket_listener);
			}
			if (socket_listener->poll_fds[0].revents & POLLNVAL) {
				socket_listener->open_fds_count--;
				/* dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking : POLLNVAL, client closed fd %d\n", socket_listener->poll_fds[0].fd); */
			}
			if (socket_listener->poll_fds[0].revents & POLLHUP ||
					   socket_listener->poll_fds[0].revents & POLLERR) {                /* POLLERR | POLLHUP */
				if (close(socket_listener->poll_fds[0].fd) == -1) {
					/* dprintf(socket_listener->log_file, "%s%s%s%s%s\n", */
					/* 		(errno==EBADF)?"EBADF":"", */
					/* 		(errno==EINTR)?"EINTR":"", */
					/* 		(errno==EIO)?"EIO":"", */
					/* 		(errno==ENOSPC)?"ENOSPC":"", */
					/* 		(errno==EDQUOT)?"EDQUOT":"" */
					/* 	); */
					dprintf(socket_listener->log_file, "[error-tabbed] loop_listen_nonblocking : can't close fd `%u`\n", socket_listener->poll_fds[0].fd);
				} else {
					/* dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking : POLLHUP or POLLERR, closed fd %d\n", socket_listener->poll_fds[0].fd); */
					socket_listener->open_fds_count--;
				}
				return 0;
			}
			return 0;
		}
	} else {
		/* dprintf(socket_listener->log_file, "[log-tabbed] loop_listen_nonblocking : all file descriptors closed; bye\n"); */
		return 1;
	}
}

int
loop_send(SocketListener* socket_listener)
{
	if (QUEUE_SIZE(socket_listener->messages_queue) > 0) {
		struct msg* message;
		QUEUE_POP(socket_listener->messages_queue, message);
		/* dprintf(socket_listener->log_file, "[log-tabbed] loop_send : sending `%s`:%ld\n to fd `%d`", */
		/* 		message->content, strlen(message->content), socket_listener->poll_fds[0].fd); */
		const size_t sizeof_msg_content = sizeof(((struct msg*)0)->content);
		if (send(socket_listener->poll_fds[0].fd, message->content,
					strnlen(message->content, sizeof_msg_content), 0) == -1) {
			dprintf(socket_listener->log_file, "[error-tabbed] socket_send : send(%u, %s) == -1\n",
					socket_listener->poll_fds[0].fd, message->content);
		}
	}
}
int
run_blocking_socket_listener(SocketListener* socket_listener)
{
	if (setup_nonblocking_listener(socket_listener) == -1) {
		dprintf(socket_listener->log_file, "[error-tabbed] run_blocking_socket_listener : setup failed, exitting...\n");
		return -1;
	} else {
		dprintf(socket_listener->log_file, "[log-tabbed] run_blocking_socket_listener : starting to listen on port `%u`\n", socket_listener->socket_port);
		// Timeout 200ms between iterations
		char recv_buffer[64];
		while (1) {
			int ret = loop_listen_nonblocking(socket_listener, recv_buffer, 63, 200);
			if (ret == -1) {
				dprintf(socket_listener->log_file, "[error-tabbed] run_blocking_socket_listener : socket loop error, exitting\n");
				return -1;
			} else if (ret == 1) {
				dprintf(socket_listener->log_file, "[log-tabbed] run_blocking_socket_listener : no more fd open, communication ended, XD !\n");
				break;
			} else if (ret == 2) {
				dprintf(socket_listener->log_file, "[log-tabbed] run_blocking_socket_listener : received `%s`\n",
						recv_buffer);
			}/* else if (ret == 0) {
				// No error and not finished, continue looping
			}*/
		}
	}
	return 0;
}

void
cleanup_socket(SocketListener* socket_listener)
{
	// Freeing and dropping unsent messages
	while (QUEUE_SIZE(socket_listener->messages_queue) > 0) {
		struct msg* message;
		QUEUE_POP(socket_listener->messages_queue, message);
		free(message);
	}
	QUEUE_FREE(socket_listener->messages_queue);
}

int
socket_send(SocketListener* socket_listener, char* message_to_send, size_t msg_size)
{
	if (socket_listener->open_fds_count <= 0) {
		/* dprintf(socket_listener->log_file, "[error-tabbed] socket_send : can't send message, no fds open\n"); */
		return -1;
	} else {
		const size_t sizeof_msg_content = sizeof(((struct msg*)0)->content);
		struct msg* message = malloc(sizeof(struct msg));
		message->content[0] = '\0';
		strlcpy(message->content, message_to_send, sizeof_msg_content);
		/* fprintf(stderr, "[log-tabbed] sent : `%s` => `%s`:`%ld`\n", */
		/* 		message_to_send, message->content, sizeof_msg_content); */
		QUEUE_PUSH(socket_listener->messages_queue, message);
		/* dprintf(socket_listener->log_file, "[log-tabbed] socket_send : message `%s` pushed\n", */
			/* message->content); */
	}
	return 0;
}

int
make_socket_listener(SocketListener* socket_listener, unsigned long UNIQUE_ID) {
	socket_listener->listening_socket_fd = 0;
	socket_listener->open_fds_count = 0;
	socket_listener->socket_port = 0;
	socket_listener->client_accepted = 0;
	socket_listener->lock_port_fd = 0;
	socket_listener->first_empty_pollin_received_time = 0;
	socket_listener->empty_pollin_received_count = 0;
	char format[64];
	format[0] = '\0';
	strlcpy(format, SOCKET_TMP_LOCK_FORMAT, 63);
	socket_listener->lock_port_fd = mkstemp(format);
	if (socket_listener->lock_port_fd == -1) {
		dprintf(socket_listener->log_file, "[error-tabbed] make_socket_listener : mkstemp(%s) == -1\n",
				SOCKET_TMP_LOCK_FORMAT);
		return -1;
	} else {
		// Delete the file while keeping valid the file descriptor
		// See https://stackoverflow.com/a/64845706
		unlink(format);
	}
	QUEUE_INIT(struct msg, socket_listener->messages_queue);
	return 0;
}

unsigned long
make_unique_id() {
	srand((unsigned int)time(NULL));
	return rand();
}

int
wait_for_socket_port_lock(SocketListener* socket_listener, int delay)
{
	struct stat statbuf = {.st_size = 0 };
	struct timespec delay_time = {
		.tv_sec = 0,
		.tv_nsec = delay*1000000
	};
	// Wait until lock file is filled with port, 200ms delay between iterations
	while (statbuf.st_size == 0) {
		if (fstat(socket_listener->lock_port_fd, &statbuf) == -1) {
			dprintf(socket_listener->log_file, "[error-tabbed] wait_for_socket_port_lock : fstat(%u) == -1\n",
					socket_listener->lock_port_fd);
			return -1;
		}
		nanosleep(&delay_time, NULL);
	}
	// fd is seeked to the end by the write call from the server,
	// rewinding it for the read
	lseek(socket_listener->lock_port_fd, 0, SEEK_SET);
	/* dprintf(socket_listener->log_file, "[log-tabbed] wait_for_socket_port_lock : server filled lock port file\n"); */
	// Reading the port number
	char port_buffer[12];
	ssize_t read_size  = read(socket_listener->lock_port_fd, port_buffer, 11);
	if (read_size == -1) {
		dprintf(socket_listener->log_file, "[error-tabbed] wait_for_socket_port_lock : lock port file can't be read, read(%u) == -1\n",
				socket_listener->lock_port_fd);
		close(socket_listener->lock_port_fd);
		return -1;
	} else {
		close(socket_listener->lock_port_fd);
		unsigned long port = strtoul(port_buffer, NULL, 10);
		if (port == ULONG_MAX) {
			dprintf(socket_listener->log_file, "[error-tabbed] wait_for_socket_port_lock : invalid socket port in lock file : strtoul(%s, 10) == ERROR\n",
					port_buffer);
			return -1;
		} else {
			return port;
		}
	}
	return 0;
}

// return posix time in ms
int64_t
get_posix_time()
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		fprintf(stderr, "[error-tabbed] get_posix_time : can't get time, clock_gettime(CLOCK_MONOTONIC) == -1\n");
		return -1;
	} else {
		return (long)ts.tv_sec * 1000L + (ts.tv_nsec / 1000000);
	}
}
