#include <stdarg.h>
#include <stdio.h>

#include "log.h"

static bool debug = 0;
static bool quiet = 0;

void set_debug(bool _debug)
{
    debug = _debug;
}

void set_quiet(bool _quiet)
{
    quiet = _quiet;
}

void logger(int priority, const char *format, ...)
{
    FILE* fout = stdout;
    va_list args;

    switch (priority)
    {
    case DEBUG:
        if (!debug || quiet)
        {
            return;
        }
        break;
    case INFO:
        if (quiet)
        {
            return;
        }
        break;
    case ERROR:
        fout = stderr;
        break;
    default:
        return;
    }

    va_start(args, format);
    vfprintf(fout, format, args);
    va_end(args);

    fprintf(fout, "\n");
}
