
#include "timerqueue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "atomic.h"

#include <assert.h>
#include <errno.h>


/************函数声明区域**************/

/**
 *  子线程的回调函数：用于处理定时器队列
 */
void *timer_queue_thread_cbk(void *arg);


/************函数实现区域**************/

static int vs_gettimeofday(struct timeval *tv, void *tzp)
{
    int ret = 0;
#ifdef CLOCK_MONOTONIC
    struct timespec ts;

    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret < 0)
        return ret;
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
#else
    ret = gettimeofday(tv, NULL);
#endif

    return ret;
}

/**
 * 主线程：创建一个定时器 (链表)
 * @param [in]  timer  定时器任务链表
 */
timer_queue_t *init_timer_queue(void)
{
    timer_queue_t *timer = malloc(sizeof(timer_queue_t));
    int           ret = -1;

    if (timer) {
        timer->stop         = 0;

        // 初始化定时器链表
        INIT_LIST_HEAD(&timer->event_list);
        // 初始化锁
        pthread_mutex_init(&timer->mutex, NULL);

        // 创建线程：处理定时器任务
        ret = pthread_create(&timer->tid, NULL, timer_queue_thread_cbk, timer);
        if (ret) {
            pthread_mutex_destroy(&timer->mutex);
            free(timer);
            timer = NULL;
        }
    }
    return timer;
}

/**
 * 主线程：释放定时器 (链表)
 * @param [in]  timer  定时器任务链表
 */
void fini_timer_queue(timer_queue_t *timer)
{
    if (timer == NULL)
        return;

    timer->stop = 1;

    // 终止定时器任务(线程)
    pthread_cancel(timer->tid);

    // pthread_join等待线程终止，释放线程资源
    pthread_join(timer->tid, NULL);

    timer_event_t   *event = NULL;
    timer_event_t   *next = NULL;

    // 遍历定时器链表，删除链表中的所有定时器任务
    list_for_each_entry_safe(event, next, &timer->event_list, list) {
        /* 定时器线程已经销毁, 若链表中还有事件，那么ref一定为1 */
        assert(atomic_read(&event->ref) == 1);
        delete_timer_event(timer, event);
    }

    // 再次断言下链表是否为空
    assert(list_empty(&timer->event_list));

    pthread_mutex_destroy(&timer->mutex);
    free(timer);
}


/**
 *    事件引用计数+1
 * @param [in]  event  定时器事件
 */
static inline void timer_event_ref(timer_event_t *event)
{
    assert(event);
    atomic_inc(&event->ref);
}

/**
 *    事件引用计数-1
 * @param [in]  event  定时器事件
 *@detail  当引用计数=0时，释放event
 */
static inline void timer_event_unref(timer_event_t *event)
{
    assert(event);

    // 引用计数-1
    if (atomic_dec_and_test(&event->ref))
        free(event);
}

/**
 *    重设任务到期时间
 * @param [in]  event  定时器事件
 * @detail  当引用计数=0时，释放event
 */
static inline void set_event_expire(timer_event_t *event)
{
    assert(event);

    vs_gettimeofday(&event->expire, NULL);
    event->expire.tv_sec += event->timeout;
}

/**
 *    将event按到期时间顺序插入到timer中的任务链表中
 * @param [in]  timer  定时器任务链表
 * @param [in]  event  定时器事件
 */
static inline void __add_timer_event(timer_queue_t *timer, timer_event_t *event)
{

    assert(timer);
    assert(event);

    timer_event_t *prev = NULL;

    // 如果链表为空，直接尾插到链表
    if (list_empty(&timer->event_list)) {
        list_add_tail(&event->list, &timer->event_list);
        return;
    }

    // [反向]遍历链表
    list_for_each_entry_reverse(prev, &timer->event_list, list) {
        // 找到小于event的prev，break
        if (prev->expire.tv_sec <= event->expire.tv_sec) {
            break;
        }
    }

    // 将event插入到prev的后面
    list_add(&event->list, &prev->list);
}

/**
 *  添加新的定时器任务
 *@param [in]  timer    定时器管理队列
 *@param [in]  timeout  重设到期时间
 *@param [in]  fn       定时器到期时，触发的回调函数
 *@param [in]  data     回调函数参数
 */
timer_event_t *add_timer_event(timer_queue_t *timer, unsigned timeout,
                               timer_event_cbk fn, void *data)
{
    if (timer == NULL || fn == NULL)
        return NULL;

    // 创建定时器事件event
    timer_event_t *event = malloc(sizeof(timer_event_t));
    if (event == NULL)
        return NULL;

    // event的赋值
    event->fn       = fn;
    event->data     = data;
    event->timeout  = timeout;  // 到期事件

    // 事件的引用设为1，表示没被处理
    atomic_set(&event->ref, 1);
    // 将任务的到期时间，增加timeout
    set_event_expire(event);

    // 将event插入到链表timer中
    pthread_mutex_lock(&timer->mutex);
    __add_timer_event(timer, event);
    pthread_mutex_unlock(&timer->mutex);

    return event;
}

/**
*  将event任务从链表timer中删除
*@param [in]  timer    定时器管理队列
*@param [in]  event    待删除的任务
*/
void delete_timer_event(timer_queue_t *timer, timer_event_t *event)
{
    if (timer == NULL || event == NULL)
        return;

    pthread_mutex_lock(&timer->mutex);
    list_del(&event->list);  // 从链表中，将event删除
    pthread_mutex_unlock(&timer->mutex);

    // 删除event后，需要将event的引用计数-1
    timer_event_unref(event);
}


/**
 *    重新设置event的到期时间，将event插入到timer链表中
 * @param [in]  timer    定时器任务链表
 * @param [in]  event    定时器事件
 * @param [in]  timeout  到期时间
 */
void set_timer_event_timeout(timer_queue_t *timer, timer_event_t *event,
                             unsigned timeout)
{
    if (event == NULL || timer == NULL)
        return;

    pthread_mutex_lock(&timer->mutex);

    // 从链表中删除event
    list_del(&event->list);

    // 重新设置到期时间
    event->timeout = timeout;
    set_event_expire(event);
    // 将event插入到timer链表中
    __add_timer_event(timer, event);

    pthread_mutex_unlock(&timer->mutex);
}


/**
 * 子线程：处理定时器链表中的到期任务
 * @detail
 *     任务队列为空
 *       睡眠1s   select(0, NULL, NULL, NULL, &sleeptv[1,0]);
 *     任务队列不为空
 *        (1) 当链表中有到期任务时，处理之
 *              - 执行到期任务的回调函数
 *              - 重设该任务的到期时间，并作为新任务重新添加到任务队列
 *        (2) 当链表中的有任务都没到期
 *              - 取出[最近将要到期]的任务，计算距离到期的时间差
 *              - 用select睡眠
 */
void *timer_queue_thread_cbk(void *arg)
{
    timer_queue_t *timer = arg;
    assert(timer);

    timer_event_t   *event = NULL;
    timer_event_t   *next = NULL;
    struct timeval  sleeptv;  // 距离[最近将要到期]的任务的时间差
    struct timeval  now;

    while (!timer->stop) {
        sleeptv.tv_sec  = 1;
        sleeptv.tv_usec = 0;

        vs_gettimeofday(&now, NULL);  // 获取当前时间now

        // 设置线程取消点属性：DISABLE (使线程在此处不能被取消)
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        pthread_mutex_lock(&timer->mutex);

        /* 遍历定时任务链表：执行已经到期的定时器任务 */
        list_for_each_entry_safe(event, next, &timer->event_list, list) {
            if (event == NULL)
                continue;
            // 若当前时间now < 最小的时间，则求出需要睡眠的时间 && break跳出链表循环
            if (now.tv_sec < event->expire.tv_sec) {
                sleeptv.tv_sec = event->expire.tv_sec - now.tv_sec;
                break;
            }

            // 获取到到期任务event，则处理之
            timer_event_ref(event);  // event引用计数+1
            event->fn(event->data);  // 执行到期任务：即调用event的回调函数

            list_del(&event->list);  // 删除任务
            set_event_expire(event); // 用timeout重设到期时间
            __add_timer_event(timer, event); // 将任务重新加入定时任务队列

            // event引用计数-1
            timer_event_unref(event);
        }

        pthread_mutex_unlock(&timer->mutex);

        // 设置线程取消点属性：ENABLE
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        // 睡眠，直到第一个到期时间到来
        select(0, NULL, NULL, NULL, &sleeptv);
    }

    return NULL;
}



