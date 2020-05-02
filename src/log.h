#ifndef LOG_H_
#define LOG_H_

#include <stdbool.h>

enum {
    DEBUG = 0,
    INFO,
    ERROR,
};

void logger(int priority, const char *format, ...);
void set_debug(bool _debug);
void set_quiet(bool _quiet);

#endif /* LOG_H_ */
