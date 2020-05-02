#ifndef SERVER_CORE_H_
#define SERVER_CORE_H_

#include <stdint.h>

#include "server_utils.h"

void server_run(char *ip, uint16_t  port);
void server_exit(void);

#endif /* SERVER_CORE_H_ */
