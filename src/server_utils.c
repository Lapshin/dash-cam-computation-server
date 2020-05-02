#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

#include "log.h"
#include "server_utils.h"

static int init_server_address(struct sockaddr_in *addr, char *ip, uint16_t port);
static int init_server_socket(struct sockaddr_in *serv_addr, int max_client_count);

int init_server(char *ip, uint16_t port, int max_client_count)
{
    int ret = -1;
    struct sockaddr_in serv_addr = {0};
    if (ip == NULL)
    {
        logger(ERROR, "ip address pointer is NULL");
        goto exit;
    }

    ret = init_server_address(&serv_addr, ip, port);
    if (ret != 0)
    {
        logger(ERROR, "Can't init server ip address");
        goto exit;
    }

    ret = init_server_socket(&serv_addr, max_client_count);
    if (ret < 0)
    {
        logger(ERROR, "Can't init server socket");
        goto exit;
    }

exit:
    return ret;
}

static int init_server_address(struct sockaddr_in *addr, char *ip, uint16_t port)
{
    int ret;
    if(ip == NULL)
    {
        return -1;
    }
    memset(addr, 0x00, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    ret = inet_pton(AF_INET, ip, &(addr->sin_addr)); // 1 - on success
    return (ret == 1) ? 0 : -1;
}

static int init_server_socket(struct sockaddr_in *serv_addr, int max_client_count)
{
    int server_fd = -1, ret = -1;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1)
    {
        logger(ERROR, "Listen failed (%d:%s)", errno, strerror(errno));
        goto error;
    }

    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret != 0) {
        logger(ERROR, "setsockopt() failed (%d, %s)", errno, strerror(errno));
        goto error;
    }

    ret = make_socket_non_blocking(server_fd);
    if (ret != 0) {
        logger(ERROR, "make_socket_non_blocking()");
        goto error;
    }

    ret = bind(server_fd, (struct sockaddr*)serv_addr, sizeof(*serv_addr));
    if(ret != 0)
    {
        logger(ERROR, "Bind failed (%d:%s)", errno, strerror(errno));
        goto error;
    }

    ret = listen(server_fd, max_client_count);
    if (ret != 0)
    {
        logger(ERROR, "Listen failed (%d:%s)", errno, strerror(errno));
        goto error;
    }

    goto exit;

error:
    if(server_fd != -1)
    {
        close(server_fd);
        server_fd = -1;
    }

exit:
    return server_fd;
}

int make_socket_non_blocking(int fd)
{
    int ret, flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        logger(ERROR, "fcntl fail (%d:%s)", errno, strerror(errno));
        return -1;
    }

    flags |= O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1)
    {
        logger(ERROR, "fcntl fail (%d:%s)", errno, strerror(errno));
    }

    return ret;
}

int accept_connection(const int server_fd)
{
    int             client_fd  = -1;
    struct sockaddr in_addr    = {0};
    socklen_t       in_len     = sizeof(in_addr);
    char hbuf[NI_MAXHOST] = {0}, sbuf[NI_MAXSERV] = {0};

    client_fd = accept(server_fd, &in_addr, &in_len);

    if (getnameinfo(&in_addr, in_len,
                    hbuf, sizeof(hbuf),
                    sbuf, sizeof(sbuf),
                    NI_NUMERICHOST | NI_NUMERICHOST) == 0) {
        logger(DEBUG, "Accepted connection on descriptor %d (host=%s, port=%s)",
               client_fd, hbuf, sbuf);
    }

    return client_fd;
}

int waiting_connection(const int server_fd)
{
    int ret = -1;
    fd_set set;

    if (server_fd >= FD_SETSIZE)
    {
        logger(ERROR, "Socket fd is out range for select. Please use epoll()");
    }

    FD_ZERO(&set);
    FD_SET(server_fd, &set);

    errno = 0;
    ret = select(server_fd + 1, &set, NULL, NULL, NULL);
    if(ret == -1)
    {
        logger(ERROR, "select failed (%d:%s)",  errno, strerror(errno));
    }

    return ret;
}

size_t read_wrapper(int fd, uint8_t *ptr, size_t size, bool read_full_size)
{
    size_t bytes_read = 0;
    do
    {
        ssize_t count = read(fd, ptr + bytes_read, size - bytes_read);
        if (count == -1)
        {
            if(errno == EAGAIN || (EWOULDBLOCK != EAGAIN && errno == EWOULDBLOCK))
            {
                continue;
            }
            logger(ERROR, "Error while reading from socket(%d:%s)",  errno, strerror(errno));
            goto exit;
        }
        if(count == 0) /* EOF - TCP connection closed */
        {
            goto exit;
        }

        bytes_read += (size_t)count;
    }
    while (read_full_size && bytes_read != size);

exit:
    return bytes_read;
}
