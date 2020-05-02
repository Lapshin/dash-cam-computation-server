#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "log.h"

#define TIMER_SIGNAL SIGRTMIN

timer_t timer_id = 0;

int init_timer(void (*timer_expired_handler)(int, siginfo_t *, void *))
{
    int ret = -1;
    struct sigevent sev;
    struct sigaction sa;

    /* Establish handler for timer signal */

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_expired_handler;
    sigemptyset(&sa.sa_mask);
    errno = 0;
    ret = sigaction(TIMER_SIGNAL, &sa, NULL);
    if (ret != 0)
    {
        logger(ERROR, "Error on sigaction (%d:%s)", errno, strerror(errno));
        goto exit;
    }
    /* Create the timer */

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIGNAL;
    sev.sigev_value.sival_ptr = &timer_id;

    ret = timer_create(CLOCK_REALTIME, &sev, &timer_id);
    if ( ret != 0)
    {
        logger(ERROR, "Error while timer create (%d:%s)", errno, strerror(errno));
    }

exit:
    return ret;
}

void deinit_timer(void)
{
    if(timer_id == 0)
    {
        return;
    }
    timer_delete(timer_id);
    timer_id = 0;
}

int timer_arm(time_t seconds)
{
    int ret = -1;
    struct itimerspec its = {0};
    its.it_value.tv_sec = seconds;

    errno = 0;
    ret = timer_settime(timer_id, 0, &its, NULL);
    if (ret != 0)
    {
        logger(ERROR, "Error while %sarming timer (%d:%s)",
               seconds ? "" : "dis", errno, strerror(errno));
    }
    return ret;
}

int timer_disarm(void)
{
    return timer_arm(0);
}
