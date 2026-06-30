#ifndef MIBLE_USER_API_H_
#define MIBLE_USER_API_H_

/* TIMER related */
typedef void (*mible_timer_handler)(void *);

typedef enum {
    MIBLE_TIMER_SINGLE_SHOT,
    MIBLE_TIMER_REPEATED,
} mible_timer_mode;

typedef enum {
    MESH_SCAN_OFF = 0,  // stop scan window
    MESH_SCAN_LOW,        // 15% scan window
    MESH_SCAN_NORMAL,   // 20% scan window
    MESH_SCAN_HIGH,        // 30% scan window
    MESH_SCAN_FULL,        // 100% scan window, enable relay
    MESH_SCAN_WIRELESS, // 2% scan window, only for wireless switch mode
    MESH_SCAN_SENSOR,   // start scan 60s/21hours for receive snb
} mesh_scan_level_t;

int mibeacon_set_adv_timeout(uint32_t timeout);

int mible_user_timer_create(void **p_timer_id, mible_timer_handler timeout_handler,
                mible_timer_mode mode);
int mible_user_timer_delete(void *timer_id);
int mible_user_timer_start(void *timer_id, uint32_t timeout_value, void *p_context);
int mible_user_timer_stop(void *timer_id);

#endif
