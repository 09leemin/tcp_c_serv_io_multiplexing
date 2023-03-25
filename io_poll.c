/*
    Filename    : io_poll.c
    Description : I/O multiplexing implementation with poll()  
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include "stdalsp.h"

#define LISTEN_BACKLOG  256
#define MAX_FD_SOCKET   0xFF

struct pollfd pollfds[MAX_FD_SOCKET];
int cnt_fd_socket;

int add_socket(int fd);
int del_socket(int fd);
void pr_socket();

int main(int argc, char **argv)
{
    /* network var */
    socklen_t len_saddr;
    int fd, fd_listener;
    int ret_recv;
    char *port, buf[1024];
    /* I/O multiplexing var */
    int ret_poll;
    /* logical var */
    int i;

    if (argc != 2) {
        printf("%s [port number]\n", argv[0]);
    }
    if (argc == 2) {
        port = strdup(argv[1]);
    } else {
        port = strdup("0");
    }

    struct addrinfo ai, *ai_ret;
    int rc_gai;
    memset(&ai, 0, sizeof(ai));
    ai.ai_family = AF_INET;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

    if ((rc_gai = getaddrinfo(NULL, port, &ai, &ai_ret)) != 0) {
        pr_err("Fail: getaddrinfo():%s", gai_strerror(rc_gai));
        exit(EXIT_FAILURE);
    }

    if ((fd_listener = socket(
                    ai_ret->ai_family,
                    ai_ret->ai_socktype,
                    ai_ret->ai_protocol)) == -1) {
        pr_err("Fail: socket()");
        exit(EXIT_FAILURE);
    }

    if (bind(fd_listener, ai_ret->ai_addr, ai_ret->ai_addrlen) == -1) {
        pr_err("Fail: bind()");
        exit(EXIT_FAILURE);
    }

	if (!strncmp(port, "0", strlen(port))) { /* random port인 경우 */
		struct sockaddr_storage saddr_s;
		len_saddr = sizeof(saddr_s);
		getsockname(fd_listener, (struct sockaddr *)&saddr_s, &len_saddr);
		if (saddr_s.ss_family == AF_INET) {
			pr_out("bind : IPv4 Port : #%d",
					ntohs(((struct sockaddr_in *)&saddr_s)->sin_port));
		} else  if (saddr_s.ss_family == AF_INET6) {
			pr_out("bind : IPv6 Port : #%d",
					ntohs(((struct sockaddr_in6 *)&saddr_s)->sin6_port));
		} else {
			pr_out("getsockname : ss_family=%d", saddr_s.ss_family);
		}
	} else {
		pr_out("bind : %s", port);
	}
    listen(fd_listener, LISTEN_BACKLOG);
    add_socket(fd_listener);

    while(1) {
        if ((ret_poll = poll(pollfds, cnt_fd_socket, -1)) == -1) {
            /* error */
        }
        pr_out("\tpoll = (%d)", ret_poll);
        if (pollfds[0].revents & POLLIN) {  // Oth pollfds에는 listener
            struct sockaddr_storage saddr_c;
            len_saddr = sizeof(saddr_c);
            if ((fd = accept(pollfds[0].fd, (struct sockaddr *)&saddr_c, &len_saddr))
                    == -1) {
                pr_err("Error get connection from listen socket");
                /* error */
                continue;
            }
            if (add_socket(fd) == -1) {
                /* error : force to disconnect. */
            }
            pr_out("accept: add socket (%d)", fd);
            continue;
        }

        for (i=1; i<cnt_fd_socket && ret_poll > 0; i++) {
            if (pollfds[i].revents & POLLIN) {
                pr_out("POLLIN: normal-inband");
                if ((ret_recv = recv(pollfds[i].fd, buf, sizeof(buf), 0)) == -1) {
                    pr_err("fd(%d) recv() error (%s)", pollfds[i].fd, strerror(errno));
                } else {
                    if (ret_recv == 0) {
                        /* closed */
                        pr_out("fd(%d): Session closed", pollfds[i].fd);
                        del_socket(pollfds[i].fd);
                        i--;    /* del_socket에 의해 전체 개수가 감소했으므로. */
                    } else {
                        /* normal */
                        pr_out("recv(fd=%d, n=%d) = %.*s",
                                pollfds[i].fd, ret_recv, ret_recv, buf);
                    }
                }
                ret_poll--;
            } else if (pollfds[i].revents & POLLERR) { /* error */
                ret_poll--;
            } else if (pollfds[i].revents & POLLNVAL) {/* invalid fd */
                ret_poll--;
            } else {
                pr_out("> No signal:fd(%d)", pollfds[i].fd);
            }
        } // loop: for(i)
    }

    return 0;
}

/*
    Name : add_socket
    Desc : append socket fd & events to pollfds
    Argv : int fd
    Ret  : positive(if success, means array index), negative(if fail)
*/
int add_socket(int fd)
{
    if (cnt_fd_socket < MAX_FD_SOCKET) {
        pollfds[cnt_fd_socket].fd = fd;
        pollfds[cnt_fd_socket].events = POLLIN;
        return ++cnt_fd_socket;
    } else {
        return -1;
    }
}

/*
    Name : del_socket
    Desc : replace pollfds
    Argv : int fd
    Ret  : positive(if success, means array index), negative(if fail)
*/
int del_socket(int fd)
{
    int i, flag = 0;    /* 1:found, 0:not found */

    close(fd);
    for (i=0; i<cnt_fd_socket; i++) {
        if (pollfds[i].fd == fd) {
            if (i != (cnt_fd_socket-1)) {
                pollfds[i] = pollfds[cnt_fd_socket-1];  /* replace pollfd to last one */
            }
            pollfds[cnt_fd_socket].fd = -1;
            flag = 1;
            break;
        }
    } // loop: for(i)

    if (flag == 0) {
        return -1;
    }
    --cnt_fd_socket;

    return i;
}

/*
    Name : pr_socket
    Desc : print socket list in poller
    Argv : None
    Ret  : None
*/
void pr_socket()
{
    int i;
    printf("> Count of socket list = %d\n", cnt_fd_socket);
    for (i=0; i<cnt_fd_socket; i++) {
        pr_out("index(%d) fd(%d)", i, pollfds[i].fd);
    }
}