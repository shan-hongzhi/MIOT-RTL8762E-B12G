#include <zephyr.h>
#include <kernel.h>
#include <bluetooth/hci.h>

#include <nuttx/wdog.h>

#include "mible.h"
#include "mible_user_api.h"

#if defined(CONFIG_MIBLE_PSK) && (CONFIG_MIBLE_PSK)
#include "mible_beacon.h"
#else
#include <bluetooth/mesh/msg.h>
#include <bluetooth/mesh/cfg.h>
#include "mible_mesh.h"
#endif

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

struct user_timer {
    struct wdog_s wdog;
    mible_timer_handler handler;
    mible_timer_mode mode;
    uint32_t ticks;
    void *ctx;
    struct mible_task_item post;
};

int mible_user_timer_create(void **p_timer_id, mible_timer_handler timeout_handler,
                mible_timer_mode mode)
{
    struct user_timer *timer;

    if (!timeout_handler || !p_timer_id) {
        return -EINVAL;
    }

    timer = k_aligned_alloc(8, sizeof(struct user_timer));
    if (!timer) {
        return -ENOMEM;
    }

    (void)memset(timer, 0, sizeof(struct user_timer));

    timer->handler = timeout_handler;
    timer->mode = mode;

    *p_timer_id = timer;

    return 0;
}

static void user_timer_expire(wdparm_t arg);

static void user_timer_expire_process(void *arg)
{
    struct user_timer *timer = arg;

    if (!timer->handler) {
        k_free(timer);
        return;
    }

    if (timer->mode == MIBLE_TIMER_REPEATED && timer->ticks) {
        (void)wd_start(&timer->wdog, timer->ticks, user_timer_expire, (wdparm_t)timer);
    }

    timer->handler(timer->ctx);
}

static void user_timer_expire(wdparm_t arg)
{
    struct user_timer *timer = (void *)arg;

    mible_single_task_post(&timer->post, user_timer_expire_process, (void *)arg);
}

int mible_user_timer_start(void *timer_id, uint32_t timeout_value, void *p_context)
{
    struct user_timer *timer = timer_id;

    if (!timer || !timer->handler || !timeout_value) {
        return -EINVAL;
    }

    timer->ticks = K_MSEC(timeout_value).ticks;
    timer->ctx = p_context;

    return wd_start(&timer->wdog, timer->ticks, user_timer_expire, (wdparm_t)timer);
}

int mible_user_timer_stop(void *timer_id)
{
    struct user_timer *timer = timer_id;

    return wd_cancel(&timer->wdog);
}

int mible_user_timer_delete(void *timer_id)
{
    struct user_timer *timer = timer_id;

    (void)wd_cancel(&timer->wdog);

    /* race condition, defer free */
    if (atomic_test_bit(&timer->post.flags, MIBLE_TASK_USED)) {
        printk("timer pending, defer free\n");
        timer->handler = NULL;
        return 0;
    }

    k_free(timer);

    return 0;
}

static void adv_timeout_handler(struct k_work *work)
{
    printk("Advertising timeout\n");

    mible_adv_suspend();
}

static K_WORK_DELAYABLE_DEFINE(adv_timer, adv_timeout_handler);

int mibeacon_set_adv_timeout(uint32_t timeout)
{
    if (!timeout) {
        (void)k_work_cancel_delayable(&adv_timer);
        return mible_adv_suspend();
    } else if (timeout == 0xFFFFFFFF) {
        (void)k_work_cancel_delayable(&adv_timer);
    } else {
        (void)k_work_reschedule(&adv_timer, K_MSEC(timeout));
    }

    return mible_adv_resume();
}
