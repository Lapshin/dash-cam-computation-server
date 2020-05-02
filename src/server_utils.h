#ifndef SERVER_UTILS_H_
#define SERVER_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

int make_socket_non_blocking(int fd);
int accept_connection(const int server_fd);
int waiting_connection(const int server_fd);
size_t read_wrapper(int fd, uint8_t *ptr, size_t size, bool read_full_size);
int init_server(char *ip, uint16_t port, int max_client_count);

#endif // SERVER_UTILS_H_
