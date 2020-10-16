#ifndef __TIMER_QUEUE_H__
#define __TIMER_QUEUE_H__

#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "list.h"

typedef int atomic_t;

// 类型：定时器事件到来时的回调函数
typedef void (*timer_event_cbk)(void *);

/* 每个定时器事件 */
typedef struct timer_event {
    struct list_head    list;     // 与其他timer_event连成链表
    struct timeval      expire;   // 到期时间
    unsigned            timeout;  // 重设到期时间
    timer_event_cbk     fn;       // 定时器事件到来时的回调函数
    void                *data;    // 回调函数的形参
    atomic_t            ref;      // timer_event的引用计数
} timer_event_t;

/* 管理所有的定时器事件: 链表 */
typedef struct timer_queue {
    pthread_t           tid;        // 定时器任务处理线程ID
    int                 stop;       // 定时器处理线程结束标志
    struct list_head    event_list; // 定时器任务链表
    pthread_mutex_t     mutex;      // 锁住event_list
} timer_queue_t;

/**
 * 主线程：创建一个定时器 (链表)
 * @param [in]  timer  定时器任务链表
 * @return 成功，timer；失败，NULL
 */
timer_queue_t *init_timer_queue(void);

/**
 * 主线程：释放定时器 (链表)
 * @param [in]  timer  定时器任务链表
 */
void fini_timer_queue(timer_queue_t *timer);

/**
 *  添加新的定时器任务
 *@param [in]  timer    定时器管理队列
 *@param [in]  timeout  重设到期时间
 *@param [in]  fn       定时器到期时，触发的回调函数
 *@param [in]  data     回调函数参数
 */
timer_event_t *add_timer_event(timer_queue_t *timer, unsigned timeout,
                               timer_event_cbk fn, void *data);

/**
*  将event任务从链表timer中删除
*@param [in]  timer    定时器管理队列
*@param [in]  event    待删除的任务
*/
void delete_timer_event(timer_queue_t *timer, timer_event_t *event);

/**
 *    重新设置event的到期时间，将event插入到timer链表中
 * @param [in]  timer    定时器任务链表
 * @param [in]  event    定时器事件
 * @param [in]  timeout  到期时间
 */
void set_timer_event_timeout(timer_queue_t *, timer_event_t *event,
                             unsigned timeout);

#endif 
