#ifndef TIMER_H_
#define TIMER_H_

int init_timer(void (*timer_expired_handler)(int, siginfo_t *, void *));
void deinit_timer(void);
int timer_arm(time_t seconds);
int timer_disarm(void);

#endif // TIMER_H_
