/*
    Filename    : io_select.c
    Description : I/O multiplexing implementation with select()
*/
#include "stdalsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define LISTEN_BACKLOG  256
#define MAX_FD_SOCKET   0xFF
#define MAX(a, b)   a > b ? a : b

int fd_socket[MAX_FD_SOCKET];   /* array for socket descriptor */
int cnt_fd_socket;              /* fd_socket array 실제 들어있는 socket descriptor 개수 */

/* I/O multiplexing var */
fd_set fds_read;
int fd_biggest;

int add_socket(int fd);
int del_socket(int fd);
int mk_fds(fd_set *fds, int *a_fd_socket);

int main(int argc, char **argv) 
{
    /* network var */
    socklen_t len_saddr;
    int fd, fd_listener;
    int ret_recv;
    char *port, buf[1024];
    /* I/O multiplexing var */
    int ret_select;
    /* logical var */
    int i;

    for (i=0; i<MAX_FD_SOCKET; i++) {
        fd_socket[i] = -1;
    }

    if (argc != 2) {
        printf("%s [port number]\n", argv[0]);
    }
    if (argc == 2) {
        port = strdup(argv[1]);
    } else {
        port = strdup("0"); // random port
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

    if (!strncmp(port, "0", strlen(port))) { /* 무작위 포트가 지정된 경우 */
        struct sockaddr_storage saddr_s;
        len_saddr = sizeof(saddr_s);
        getsockname(fd_listener, (struct sockaddr *)&saddr_s, &len_saddr);
        if (saddr_s.ss_family == AF_INET) {
            pr_out("bind: IPv4 Port : #%d",
                    ntohs(((struct sockaddr_in *)&saddr_s)->sin_port));
        } else if (saddr_s.ss_family == AF_INET6) {
            pr_out("bind: IPv6 Port : #%d",
                    ntohs(((struct sockaddr_in6 *)&saddr_s)->sin6_port));
        } else {
            pr_out("getsockname: ss_family=%d", saddr_s.ss_family);
        }
    } else {    /* 포트를 지정한 경우 */
        pr_out("bind: %s", port);
    }
    listen(fd_listener, LISTEN_BACKLOG);
    add_socket(fd_listener);    /* 감시할 소켓 리스트에 리스너 소켓 등록 */

    while(1) {
        /* waiting for data (no timeout) */
        fd_biggest = mk_fds(&fds_read, fd_socket);
        if ((ret_select = select(fd_biggest+1, &fds_read, NULL, NULL, NULL)) == -1) {
            /* error */
        }
        pr_out("\tselect = (%d)", ret_select);

        /* IS New connection ? */
        if (FD_ISSET(fd_listener, &fds_read)) { /*리스너 소켓에 이벤트가 있는가?*/
            struct sockaddr_storage saddr_c;

            /* Accept only one connection allowed per cycle */
            len_saddr = sizeof(saddr_c);
            fd = accept(fd_listener, (struct sockaddr *)&saddr_c, &len_saddr);
            if (fd == -1) {
                pr_err("Error get connection from listen socket");
                continue;
            }
            if (add_socket(fd) == -1) {
                /* error : force to disconnet */
            }
            pr_out("accept: add socket (%d)", fd);
            continue;   /* 새로운 소켓이 추가되었으므로 다시 select를 호출하는 곳으로 간다*/
        }

        for(i=1; i<cnt_fd_socket; i++) {
            if(FD_ISSET(fd_socket[i], &fds_read)) { /* 일반 클라이언트와 연결된 소켓에 대한 이벤트 검사 */
                pr_out("FD_ISSET: normal-inband");

                ret_recv = recv(fd_socket[i], buf, sizeof(buf), 0);
                if (ret_recv == -1) {
                    pr_err("fd(%d) recv() error (%s)", fd_socket[i], strerror(errno));
                } else {
                    if (ret_recv == 0) {
                        /* closed */
                        pr_out("fd(%d): Session closed", fd_socket[i]);
                        del_socket(fd_socket[i]);
                    } else {
                        /* normal */
                        pr_out("recv(fd=%d, n=%d) = %.*s",
                                fd_socket[i], ret_recv, ret_recv, buf);
                    }
                }
            }
        } /* loop: for */
    } /* loop: while */
    
    return 0;
}

/*
    Name: add_socket
    Desc: add socket descriptor to internal fd array
    Argv: int fd
    Ret : positive(if success, means array index), negative(if fail)
*/
int add_socket(int fd) 
{
    if (cnt_fd_socket < MAX_FD_SOCKET) {
        fd_socket[cnt_fd_socket] = fd;
        return ++cnt_fd_socket;
    } else {
        return -1;
    }
}

/*
    Name: del_socket
    Desc: substract socket descriptor from internal fd array
    Argv: int fd
    Ret : positive(if success, means array index), negative(if fail)
*/
int del_socket(int fd)
{
    int i, flag;
    flag = 0;   /* 1:found, 0:not found */

    close(fd);
    for (i=0; i<cnt_fd_socket; i++) {
        if (fd_socket[i] == fd) {
            if (i != (cnt_fd_socket-1)) fd_socket[i] = fd_socket[cnt_fd_socket-1];
            fd_socket[cnt_fd_socket-1] = -1;
            flag = 1;
            break;
        }
    }

    if (flag == 0) {
        return -1;
    }
    --cnt_fd_socket;

    return i;
}

/*
    Name: mk_fds
    Desc: make fd set
    Argv: fd_set *fds, int *a_fd_socket
    Ret : positive value (means biggest fd number)
*/
int mk_fds(fd_set *fds, int *a_fd_socket)
{
    int i, fd_max;
    FD_ZERO(fds);
    for (i=0, fd_max = -1; i<cnt_fd_socket; i++) {
        fd_max = MAX(fd_max, a_fd_socket[i]);
        FD_SET(a_fd_socket[i], fds);
    }
    return fd_max;
}