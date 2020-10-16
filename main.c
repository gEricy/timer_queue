#include <stdio.h>
#include <unistd.h>
#include "timerqueue.h"
#include <signal.h>

static int s_exit = 0;

void signal_cbk(int signo)
{
    s_exit = 1;
}

void cbk(void *arg)
{
    printf("cbk[%d]\n", *(int *)arg);
}

int main()
{
    signal(SIGINT, signal_cbk);

    timer_queue_t *timer = init_timer_queue();

    int a = 1, b = 2, c = 3, d = 4;
    add_timer_event (timer, a, cbk, (void *)&a);
    add_timer_event (timer, b, cbk, (void *)&b);
    add_timer_event (timer, c, cbk, (void *)&c);
    add_timer_event (timer, d, cbk, (void *)&d);

    /* 执行其他的操作 */

    while (s_exit == 0) {};

    fini_timer_queue(timer);

    return 0;
}