/****************************************************************************
 * vendor/realtek/chips/rtl8762e/rtl876x_boot.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mm/mm.h>
#include <nuttx/arch.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/clock.h>
#include <nuttx/timers/arch_alarm.h>
#include <nuttx/power/pm.h>
#include <sys/prctl.h>
#include <inttypes.h>
#include <assert.h>
#include <debug.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <nvic.h>
#include <time.h>
#include <assert.h>
#include "sched/sched.h"
#include "clock/clock.h"
#include "arm_internal.h"
#include "wdog/wdog.h"
#include "mem_config.h"
#include "mem_types.h"
#include "patch_os.h"
#include "system_rtl876x.h"
#include "irq.h"
#include "trace.h"
#include "otp.h"
#include "dlps.h"
#include "platform_utils.h"
#include "app_section.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define APP_START_ADDR   0x200000
#define APP_START_SIZE   0xE8

#define APP_TEXT_ADDR    0x203200
#define APP_TEXT_SIZE    0x5D0
#define BUFFER_HEAP_SIZE (0x3870)

#define IDLE_STACK ((uint32_t)_ebss + CONFIG_IDLETHREAD_STACKSIZE)

#define EXCLUDE_MAGIC_NUMBER  (0x5a5b5c5d)

typedef CODE void (*timer_callback)(void *);
typedef struct timer_info
{
  struct work_s work;
  clock_t ticks;
  const char *name;
  uint32_t parameter;
  timer_callback cb;
  uint8_t id;
  bool is_reload;
  bool is_ll_sup;
  bool is_running;
#ifdef CONFIG_PM
  uint32_t exclude_magic;
#endif
} TIMER_INFO;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* _sbss is the start of the BSS region (see the linker script) _ebss is the
 * end of the BSS region (see the linker script). The idle task stack starts
 * at the end of BSS and is of size CONFIG_IDLETHREAD_STACKSIZE.  The IDLE
 * thread is the thread that the system boots on and, eventually, becomes the
 * idle, do nothing task that runs only when there is nothing else to run.
 * The heap continues from there until the configured end of memory.
 * g_idle_topstack is the beginning of this heap region (not necessarily
 * aligned).
 */

#ifdef CONFIG_PM
extern uint32_t __rtl_platform_rtc_get_counter(void);
static void systick_pm_notify(struct pm_callback_s *cb, int domain,
                          enum pm_state_e pmstate);
static struct pm_callback_s g_systick_pm =
{
  .notify     = systick_pm_notify,
};
static volatile uint32_t systick_elaps_value;
static volatile uint32_t rtc_enter_dlps_value;
#endif

extern void __rtl_low_stack_task(void *no_param);

const uintptr_t g_idle_topstack = IDLE_STACK;
#if defined(CONFIG_RTL876x_BT)
static uint32_t timer_id_mask = 0xffffffff;
#endif

extern uint32_t *__buf_dataon_bss_rw_start__;
extern uint32_t *__buf_dataon_bss_rw_end__;
extern uint32_t *__buf_dataon_length__;

static uint32_t g_rtl_buffer_ram_offset = (uint32_t)&__buf_dataon_length__;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rtl_malloc,rtl_zalloc,rtl_zaligned_alloc,rtl_free,
 * rtl_nuttx_mm_initialize
 *
 * Description:
 *   Wrapper memory manager
 *
 ****************************************************************************/
static void * rtl_buffer_heap_alloc(size_t size)
{
  void * ret;

  /* align.8 */
  g_rtl_buffer_ram_offset += 0x07;
  g_rtl_buffer_ram_offset &= ~((uint32_t)0x07);

  if (g_rtl_buffer_ram_offset + size > BUFFER_HEAP_SIZE)
    {
      return NULL;
    }

  ret = (void *)((uint32_t)&__buf_dataon_bss_rw_start__ + g_rtl_buffer_ram_offset);

  g_rtl_buffer_ram_offset += size;

  return ret;
}

static bool rtl_malloc(RAM_TYPE ram_type, size_t size, const char *p_func,
                uint32_t file_line, void **pp)
{
  if(ram_type == RAM_TYPE_BUFFER_ON)
  {
    *pp = rtl_buffer_heap_alloc(size);
  }
  else
  {
    *pp = mm_memalign(g_mmheap, 8, size);
  }

  return true;
}

static bool rtl_zalloc(RAM_TYPE ram_type, size_t size,
                const char *p_func, uint32_t file_line, void **pp)
{
  if (ram_type == RAM_TYPE_BUFFER_ON)
  {
    *pp = NULL;
    return false;
  }

  *pp = mm_memalign(g_mmheap, 8, size);
  if (*pp)
    {
      (void)memset(*pp, 0, size);
    }

  return true;
}

static bool rtl_zaligned_alloc(RAM_TYPE ram_type, size_t size, uint8_t alignment,
                        const char *p_func, uint32_t file_line, void **pp)
{
  if(ram_type == RAM_TYPE_BUFFER_ON)
  {
    *pp = NULL;
    return false;
  }

  *pp = mm_memalign(g_mmheap, alignment, size);
  if (*pp)
    {
      (void)memset(*pp, 0, size);
    }

  return true;
}

static bool rtl_free(void *p_block)
{
  mm_free(g_mmheap, p_block);

  return true;
}

#ifdef CONFIG_HEAP_COLORATION
static inline void up_heap_color(void *start, size_t size)
{
  memset(start, HEAP_COLOR, size);
}
#else
#define up_heap_color(start, size)
#endif

void up_allocate_heap(void **heap_start, size_t *heap_size)
{
  /* Start with the first SRAM region */

  *heap_start = (void *)g_idle_topstack;
  *heap_size = (DATA_ON_END_ADDR - g_idle_topstack);

  /* Colorize the heap for debug */

  up_heap_color(*heap_start, *heap_size);
}

/****************************************************************************
 * Name: arm_addregion
 *
 * Description:
 *   Memory may be added in non-contiguous chunks.  Additional chunks are
 *   added by calling this function.
 *
 ****************************************************************************/

#if CONFIG_MM_REGIONS > 1
void arm_addregion(void)
{
  void *region_start;
  size_t region_size;

  /* NOTE: add 1KB to avoid conflicts of initial stack */

  region_start = (void *)APP_TEXT_ADDR;
  region_size = (uint32_t)APP_TEXT_SIZE;

  mm_addregion(g_mmheap, region_start, region_size);
}

void rtl_addregion(void)
{
  void *region_start;
  size_t region_size;

  region_start = (void *)APP_START_ADDR;
  region_size = (uint32_t)APP_START_SIZE;

  mm_addregion(g_mmheap, region_start, region_size);

  region_start = (void *)((uint32_t)&__buf_dataon_bss_rw_start__ + g_rtl_buffer_ram_offset);
  region_size = (uint32_t)BUFFER_HEAP_SIZE - g_rtl_buffer_ram_offset;

  mm_addregion(g_mmheap, region_start, region_size);
}
#endif

/****************************************************************************
 * Name: rtl_task_create，rtl_task_handle_get, rtl_nuttx_sched_initialize
 *
 * Description:
 *   Wrapper task manager
 *
 ****************************************************************************/
#ifdef CONFIG_RTL876x_BT
static struct work_s pending_work;
static sq_queue_t pending_queue;

static bool rtl_task_create(void **pp_handle, const char *p_name,
                     void (*p_routine)(void *), void *p_param,
                     uint16_t stack_size, uint16_t priority,
                     bool *p_is_create_success)
{
  struct sched_param param = {
    .sched_priority = CONFIG_SCHED_HPWORKPRIORITY,
  };
  pthread_attr_t pattr;
  pthread_t pid;

  pthread_attr_init(&pattr);
  pthread_attr_setschedparam(&pattr, &param);
  pthread_attr_setstacksize(&pattr, stack_size);

  // create controller task part
  pthread_create(&pid, &pattr, (void * (*)(void *))p_routine, p_param);
  if (pid <= 0)
    {
      return false;
    }

  prctl(PR_SET_NAME_EXT, p_name, pid);

  *pp_handle = (void *)pid;
  return true;
}
#endif

static bool rtl_task_handle_get(void **pp_handle, bool *p_result)
{
  if (pp_handle == NULL)
    {
      *p_result = false;
      return true;
    }

  *pp_handle = this_task();
  *p_result = true;

  return true;
}

DATA_RAM_FUNCTION
static bool rtl_sched_is_start(bool *p_result)
{
  *p_result = true;
  return true;
}

DATA_RAM_FUNCTION
static bool rtl_sched_state_get(bool *p_result)
{
  *p_result = 0;
  return true;
}

static bool rtl_sched_suspend(bool *p_result)
{
  if (sched_lock() == OK)
    {
      *p_result = true;
    }
  else
    {
      *p_result = false;
    }

  return true;
}

static bool rtl_sched_resume(bool *p_result)
{
  if (sched_unlock() == OK)
    {
      *p_result = true;
    }
  else
    {
      *p_result = false;
    }

  return true;
}

/****************************************************************************
 * Name: rtl_lock,rtl_unlock,rtl_sem_create,rtl_sem_delete,rtl_sem_task,
 *       rtl_sem_delete,rtl_nuttx_sem_initialize
 *
 * Description:
 *   Wrapper sem manager
 *
 ****************************************************************************/

DATA_RAM_FUNCTION
static bool rtl_lock(uint32_t *p_flags)
{
  if (__get_IPSR() != 2)
  {
    *p_flags = enter_critical_section();
    sched_lock();
  }

  return true;
}

DATA_RAM_FUNCTION
static bool rtl_unlock(uint32_t flags)
{
  if (__get_IPSR() != 2)
  {
    sched_unlock();
    leave_critical_section(flags);
  }

  return true;
}

static bool rtl_sem_create(void **pp_handle, uint32_t init_count,
                    uint32_t max_count, bool *p_result)
{
  int ret;
  sem_t *sem = NULL;
  int tmp;

  *p_result = true;
  tmp = sizeof(sem_t);
  sem = kmm_malloc(tmp);
  if (!sem)
    {
      *p_result = false;
    }
  else
    {
      ret = nxsem_init(sem, 0, init_count);
      if (ret)
        {
          kmm_free(sem);
          sem = NULL;
          *p_result = false;
        }
    }

  *pp_handle = sem;

  return true;
}

static bool rtl_sem_delete(void *p_handle, bool *p_result)
{
  sem_t *sem = (sem_t *)p_handle;

  nxsem_destroy(sem);
  kmm_free(sem);
  *p_result = true;
  return true;
}

static bool rtl_sem_take(void *p_handle, uint32_t wait_ms, bool *p_result)
{
  int ret;
  uint32_t ticks;
  sem_t *sem = (sem_t *)p_handle;
  *p_result = true;

  if (wait_ms == 0xffffffff)
    {
      ret = nxsem_wait(sem);
      if (ret)
        {
          *p_result = false;
        }
    }
  else
    {
      ticks = wait_ms / MSEC_PER_TICK;
      ret = nxsem_tickwait(sem, ticks);
      if (ret)
        {
          *p_result = false;
        }
    }

  return true;
}

static bool rtl_sem_give(void *p_handle, bool *p_result)
{
  int ret;
  sem_t *sem = (sem_t *)p_handle;
  *p_result = true;

  ret = nxsem_post(sem);
  if (ret)
    {
      *p_result = false;
    }

  return true;
}

bool rtl_mutex_create(void **pp_handle, bool *p_result)
{
  rmutex_t *p_mutex = NULL;
  int ret;

  p_mutex = (void *)mm_memalign(g_mmheap, 8, sizeof(rmutex_t));
  if (!p_mutex)
    {
      *p_result = false;
      return true;
    }

  (void)memset(p_mutex, 0, sizeof(rmutex_t));

  ret = nxrmutex_init(p_mutex);
  if(ret == 0)
    {
      *p_result = true;
      *pp_handle = p_mutex;
      return true;
    }

  mm_free(g_mmheap, p_mutex);

  return true;
}

static bool rtl_mutex_delete(void *p_handle, bool *p_result)
{
  if (nxrmutex_destroy(p_handle) == 0)
  {
    *p_result = true;
  }
  else
  {
    *p_result = false;
  }

  mm_free(g_mmheap, p_handle);

  return true;
}

static bool rtl_mutex_take(void *p_handle, uint32_t wait_ms, bool *p_result)
{
  if (nxrmutex_timedlock(p_handle, wait_ms*1000000) == 0)
  {
    *p_result = true;
  }
  else
  {
    *p_result = false;
  }
  return true;
}

static bool rtl_mutex_give(void *p_handle, bool *p_result)
{
  if (nxrmutex_unlock(p_handle) == 0)
  {
    *p_result = true;
  }
  else
  {
    *p_result = false;
  }
  return true;
}

/****************************************************************************
 * Name: rtl_timer_create, rtl_timer_callback
 *
 * Description:
 *   Wrapper task manager
 *
 ****************************************************************************/
#ifdef CONFIG_RTL876x_BT

#ifdef CONFIG_PM
static const char * const exclude_timer_name[]=
{
  "internal_32k_cal",
  "ext_adv_sched_mon_timer",
  "dbg_tid_timer",
  "THERMAL_TIMER",
  "feature_check_timer",
  "ext_adv_sched_mon_timer",
  NULL
};
#endif

#if 0
#define _RTL_TRACE_MASK (0x03ff)
uint32_t _rtl_trace[_RTL_TRACE_MASK + 1];
uint16_t _systick_next;

#define _RTL_TRACE_UNIT(c, val) \
  _rtl_trace[_systick_next++ & _RTL_TRACE_MASK] = \
                  (((uint32_t)c << 24) | ((uint32_t)val & 0x00ffffff))
#else
#define _RTL_TRACE_UNIT(c, val)
#endif

static bool rtl_timer_create(void **pp_handle, const char *p_timer_name,
                      uint32_t timer_id, uint32_t interval_ms,
                      bool reload, void (*p_timer_callback)(void),
                      bool *p_result)
{
  TIMER_INFO *timer = NULL;

  if (!timer_id_mask)
    {
      *p_result = false;
      return true;
    }

  timer = (void *)mm_memalign(g_mmheap, 8, sizeof(TIMER_INFO));
  if (!timer)
    {
      *p_result = false;
      return true;
    }

  (void)memset(timer, 0, sizeof(TIMER_INFO));

  timer->ticks = interval_ms / MSEC_PER_TICK;
  timer->cb = (timer_callback)p_timer_callback;
  timer->parameter = timer_id;
  timer->name = p_timer_name;
  timer->is_reload = reload;
  timer->id = 31 - __builtin_clz(timer_id_mask);
  timer_id_mask &= ~BIT(timer->id);

  if (!memcmp(p_timer_name, "ll_sup_timer", 12))
    {
      timer->is_ll_sup = true;
    }
#ifdef CONFIG_PM
  else
    {
      size_t name_len = strlen(p_timer_name);

      for (int i = 0; exclude_timer_name[i]; i++)
        {
          if (name_len != strlen(exclude_timer_name[i]))
            {
              continue;
            }
          else if (memcmp(p_timer_name, exclude_timer_name[i], name_len))
            {
              continue;
            }

          timer->exclude_magic = EXCLUDE_MAGIC_NUMBER;

          break;
        }
    }
#endif

  _RTL_TRACE_UNIT('C', timer);

  *pp_handle = timer;
  *p_result = true;

  return true;
}

static bool rtl_timer_start(void **pp_handle, bool *p_result);

static void rtl_timer_callback(void * arg)
{
  irqstate_t flags = enter_critical_section();
  TIMER_INFO *timer = (TIMER_INFO *)(arg);
  sq_entry_t *entry = (FAR sq_entry_t *)(timer);

  if (timer == NULL || timer->is_running == false)
    {
      leave_critical_section(flags);
      return;
    }

  if (timer->is_reload)
    {
      _RTL_TRACE_UNIT('r', timer);

      (void)work_queue(HPWORK, &timer->work, rtl_timer_callback,
                       timer, timer->ticks);
    }

  if (timer->is_ll_sup &&
      sq_peek(&pending_queue) != entry &&
      sq_tail(&pending_queue) != entry &&
      !sq_next(entry))
    {
      clock_t ticks;
      up_timer_gettick(&ticks);

      if (ticks < timer->ticks)
        {
          _RTL_TRACE_UNIT('t', timer);

          (void)work_queue(HPWORK, &timer->work, rtl_timer_callback,
                           timer, timer->ticks - ticks);

          leave_critical_section(flags);
          return;
        }
    }

  leave_critical_section(flags);

  if (timer->cb)
    {
      _RTL_TRACE_UNIT('T', timer);

      timer->cb(timer);
    }
}

static void work_pending_cb(void * arg)
{
  struct work_s *work;
  irqstate_t flags = enter_critical_section();

  while ((work = (struct work_s *)sq_remfirst(&pending_queue)) != NULL)
    {
      TIMER_INFO *timer = (TIMER_INFO *)(work);

      timer->is_running = true;

      _RTL_TRACE_UNIT('P', timer);

      (void)work_queue(HPWORK, work, rtl_timer_callback,
                       timer, timer->ticks);
    }

  leave_critical_section(flags);
}

static bool rtl_timer_start(void **pp_handle, bool *p_result)
{
  if (pp_handle == NULL || *pp_handle == NULL)
    {
      *p_result = false;
      return true;
    }

  irqstate_t flags = enter_critical_section();

  TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);
  sq_entry_t *entry = (FAR sq_entry_t *)(timer);

  if (timer->work.worker)
    {
      /* already working */
      _RTL_TRACE_UNIT('c', timer);
      work_cancel(HPWORK, &timer->work);
    }
  else if (sq_peek(&pending_queue) == entry ||
           sq_tail(&pending_queue) == entry ||
           sq_next(entry))
    {
      /* already pending */
      _RTL_TRACE_UNIT('p', timer);
      leave_critical_section(flags);
      return true;
    }
  else
    {
      _RTL_TRACE_UNIT('s', timer);
    }

  /* indicate timer pending. */
  timer->is_running = false;

  sq_addlast(entry, &pending_queue);

  leave_critical_section(flags);

  (void)work_queue(HPWORK, &pending_work, work_pending_cb, NULL, 0);

  return true;
}

static bool rtl_timer_restart(void **pp_handle, uint32_t interval_ms,
                       bool *p_result)
{
  if ((pp_handle == NULL) || (*pp_handle == NULL))
    {
      *p_result = false;
      return true;
    }

  TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);

  timer->ticks = interval_ms / MSEC_PER_TICK;

  if (timer->is_ll_sup && timer->work.worker)
    {
      clock_t ticks;
      up_timer_gettick(&ticks);

      timer->ticks += ticks;

      *p_result = true;
      return true;
    }

  return rtl_timer_start(pp_handle, p_result);
}

static bool rtl_timer_stop(void **pp_handle, bool *p_result)
{
  if ((pp_handle == NULL) || (*pp_handle == NULL))
    {
      *p_result = false;
      return true;
    }

  irqstate_t flags = enter_critical_section();

  TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);
  sq_entry_t *entry = (FAR sq_entry_t *)(timer);

  if (timer->work.worker)
    {
      /* We are later,,, */
      _RTL_TRACE_UNIT('l', timer);
      (void)work_cancel(HPWORK, &timer->work);
      goto restore;
    }
  else if (sq_peek(&pending_queue) == entry ||
           sq_tail(&pending_queue) == entry ||
           sq_next(entry))
    {
      _RTL_TRACE_UNIT('a', timer);
      sq_rem(entry, &pending_queue);
    }
  else
    {
      _RTL_TRACE_UNIT('o', timer);
    }

  if (!sq_peek(&pending_queue))
    {
      /* No need task switch, due to nothing to do. */
      (void)work_cancel(HPWORK, &pending_work);
    }

restore:
  timer->is_running = false;
  leave_critical_section(flags);

  *p_result = true;
  return true;
}

static bool rtl_timer_delete(void **pp_handle, bool *p_result)
{
  if ((pp_handle == NULL) || (*pp_handle == NULL))
    {
      *p_result = true;
      return true;
    }

  TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);

  _RTL_TRACE_UNIT('D', timer);

  (void)rtl_timer_stop(pp_handle, p_result);

  timer_id_mask |= BIT(timer->id);

  mm_free(g_mmheap, timer);

  *pp_handle = NULL;
  *p_result = true;
  return true;
}

static bool rtl_timer_id_get(void **pp_handle, uint32_t *p_timer_id,
                      bool *p_result)
{
  TIMER_INFO *timer;
  *p_result = false;

  if (pp_handle && *pp_handle)
    {
      timer = (TIMER_INFO *)(*pp_handle);
      *p_timer_id = timer->parameter;

      *p_result = true;
    }

  return true;
}

static bool rtl_timer_number_get(void **pp_handle, uint32_t *p_timer_num,
                          bool *p_result)
{
  if (pp_handle && *pp_handle)
    {
      TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);

      *p_timer_num = timer->id;
      *p_result = true;
    }
  else
    {
      *p_timer_num = 0xff;
      *p_result = false;
    }

  return true;
}

static bool rtl_timer_next_timeout_value_get(uint32_t *p_value)
{
  *p_value = 0xffffffff;

  return true;
}

static bool rtl_time_get_timer_state(void **pp_handle, uint32_t *p_timer_state, bool *p_result)
{
  if (pp_handle && *pp_handle)
    {
      TIMER_INFO *timer = (TIMER_INFO *)(*pp_handle);
      *p_timer_state = !work_available(&timer->work);
      *p_result = true;
    }
  else
    {
      *p_result = false;
    }
  return true;
}

static bool rtl_timer_pendcall(void* xFunctionToPend, void *para1,
                                                  uint32_t para2,
                                                  bool *p_result)
{
  *p_result = true;
  return true;
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
}
#endif

/****************************************************************************
 * Function:  up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize
 *   the timer interrupt.
 *
 ****************************************************************************/

#define SYSTICK_CLOCK_HZ    (32000)
#define SYSTICK_RELOAD      ((SYSTICK_CLOCK_HZ / CLOCKS_PER_SEC) - 1)

#define COUNTER_MAX         0x00ffffff
#define TIMER_STOPPED       0xff000000

#define NSEC_PER_CYC        (NSEC_PER_SEC / SYSTICK_CLOCK_HZ)

#define CYC_PER_TICK        (SYSTICK_CLOCK_HZ / CLOCKS_PER_SEC)
#define MAX_TICKS           ((COUNTER_MAX / CYC_PER_TICK) - 1)
#define MAX_CYCLES          (MAX_TICKS * CYC_PER_TICK)

/* Minimum cycles in the future to try to program.  Note that this is
 * NOT simply "enough cycles to get the counter read and reprogrammed
 * reliably" -- it becomes the minimum value of the LOAD register, and
 * thus reflects how much time we can reliably see expire between
 * calls to elapsed() to read the COUNTFLAG bit.  So it needs to be
 * set to be larger than the maximum time the interrupt might be
 * masked.  Choosing a fraction of a tick is probably a good enough
 * default, with an absolute minimum of 1k cyc.
 */
#define MIN_DELAY           MAX(SYSTICK_RELOAD, CYC_PER_TICK)

/*
 * This local variable holds the amount of SysTick HW cycles elapsed
 * and it is updated in sys_clock_isr() and sys_clock_set_timeout().
 *
 * Note:
 *  At an arbitrary point in time the "current" value of the SysTick
 *  HW timer is calculated as:
 *
 * t = cycle_counter + elapsed();
 */
static uint64_t cycle_count;

/*
 * This local variable holds the amount of elapsed HW cycles due to
 * SysTick timer wraps ('overflows') and is used in the calculation
 * in elapsed() function, as well as in the updates to cycle_count.
 *
 * Note:
 * Each time cycle_count is updated with the value from overflow_cyc,
 * the overflow_cyc must be reset to zero.
 */
static volatile uint32_t overflow_cyc;

#if 0
#define _SYSTICK_TRACE_MASK (0x07ff)
uint32_t _systick_trace[_SYSTICK_TRACE_MASK + 1];
uint16_t _systick_next;

#define _TRACE_UNIT(c, val) \
  _systick_trace[_systick_next++ & _SYSTICK_TRACE_MASK] = \
                  (((uint32_t)c << 24) | (val & 0x00ffffff))
#else
#define _TRACE_UNIT(c, val)
#endif

/* This internal function calculates the amount of HW cycles that have
 * elapsed since the last time the absolute HW cycles counter has been
 * updated. 'cycle_count' may be updated either by the ISR, or when we
 * re-program the SysTick.LOAD register, in sys_clock_set_timeout().
 *
 * Additionally, the function updates the 'overflow_cyc' counter, that
 * holds the amount of elapsed HW cycles due to (possibly) multiple
 * timer wraps (overflows).
 *
 * Prerequisites:
 * - reprogramming of SysTick.LOAD must be clearing the SysTick.COUNTER
 *   register and the 'overflow_cyc' counter.
 * - ISR must be clearing the 'overflow_cyc' counter.
 * - no more than one counter-wrap has occurred between
 *     - the timer reset or the last time the function was called
 *     - and until the current call of the function is completed.
 * - the function is invoked with interrupts disabled.
 */
static uint32_t elapsed(uint32_t *curr)
{
  uint32_t load = SysTick->LOAD + 1;
  uint32_t val1 = SysTick->VAL; /* A */
  uint32_t ctrl = SysTick->CTRL;  /* B */
  uint32_t val2 = SysTick->VAL; /* C */

  /* Save current value, use to calculate elapsed consume time */
  if (curr) {
    *curr = val2;
  }

  /* Nothing to do... */
  if (!val1 && !(ctrl & SysTick_CTRL_COUNTFLAG_Msk)) {
    _TRACE_UNIT('E', 0);
    return overflow_cyc;
  }

  /* SysTick behavior: The counter wraps at zero automatically,
   * setting the COUNTFLAG field of the CTRL register when it
   * does.  Reading the control register automatically clears
   * that field.
   *
   * If the count wrapped...
   * 1) Before A then COUNTFLAG will be set and val1 >= val2
   * 2) Between A and B then COUNTFLAG will be set and val1 < val2
   * 3) Between B and C then COUNTFLAG will be clear and val1 < val2
   * 4) After C we'll see it next time
   *
   * So the count in val2 is post-wrap and last_load needs to be
   * added if and only if COUNTFLAG is set or val1 < val2.
   */
  if ((ctrl & SysTick_CTRL_COUNTFLAG_Msk)
      || (val1 < val2)
      || !(val2)) {
    overflow_cyc += load;

    _TRACE_UNIT('O', overflow_cyc);

    /* We know there was a wrap, but we might not have
     * seen it in CTRL, so clear it. */
    (void)SysTick->CTRL;
  }

  _TRACE_UNIT('L', load);
  _TRACE_UNIT('V', val2);

  if (!val2) {
    return overflow_cyc;
  }

  return (load - val2) + overflow_cyc;
}

static int rtl876x_systick_oneshot_max_delay(struct oneshot_lowerhalf_s *lower,
                                        struct timespec *ts)
{
  ts->tv_sec  =  (time_t)(COUNTER_MAX / SYSTICK_CLOCK_HZ);
  ts->tv_nsec =  (COUNTER_MAX % SYSTICK_CLOCK_HZ) * NSEC_PER_CYC;

  return OK;
}

#ifndef MAX
/**
 * @brief Obtain the maximum of two values.
 *
 * @note Arguments are evaluated twice. Use Z_MAX for a GCC-only, single
 * evaluation version
 *
 * @param a First value.
 * @param b Second value.
 *
 * @returns Maximum value of @p a and @p b.
 */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
/**
 * @brief Obtain the minimum of two values.
 *
 * @note Arguments are evaluated twice. Use Z_MIN for a GCC-only, single
 * evaluation version
 *
 * @param a First value.
 * @param b Second value.
 *
 * @returns Minimum value of @p a and @p b.
 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/**
 * @brief Clamp a value to a given range.
 *
 * @note Arguments are evaluated multiple times. Use Z_CLAMP for a GCC-only,
 * single evaluation version.
 *
 * @param val Value to be clamped.
 * @param low Lowest allowed value (inclusive).
 * @param high Highest allowed value (inclusive).
 *
 * @returns Clamped value.
 */
#define CLAMP(val, low, high) (((val) <= (low)) ? (low) : MIN(val, high))

int up_timer_gettick(clock_t *ticks)
{
  irqstate_t flags = enter_critical_section();

  uint64_t cyc = elapsed(NULL) + cycle_count;

#if CYC_PER_TICK == 32
  *ticks = (clock_t)(cyc >> 5);
#else
  *ticks = (clock_t)(cyc / CYC_PER_TICK);
#endif

  _TRACE_UNIT('t', *ticks);

  leave_critical_section(flags);

  return OK;
}

static bool oneshot_running;

#ifdef CONFIG_PM
static clock_t g_stop_time;
#endif

int up_alarm_tick_cancel(clock_t *ticks)
{
  oneshot_running = false;

  _TRACE_UNIT('c', cycle_count);

#ifdef CONFIG_PM
  up_timer_gettick(&g_stop_time);
  *ticks = g_stop_time;
  return OK;
#else
  return up_timer_gettick(ticks);
#endif
}

int up_alarm_tick_start(clock_t ticks)
{
  uint32_t val1, last_load_ = SysTick->LOAD + 1;
  uint32_t elaps = elapsed(&val1);
  uint32_t delay = 0, val2;
  clock_t now;

  cycle_count += elaps;
  overflow_cyc = 0;

#if CYC_PER_TICK == 32
  now = cycle_count >> 5;
#else
  now = cycle_count / CYC_PER_TICK;
#endif

  _TRACE_UNIT('N', now);
  _TRACE_UNIT('T', ticks);

  if (now < ticks)
    {
      ticks -= now;
      ticks = CLAMP(ticks, 1, (int32_t)MAX_TICKS);
      /* Desired delay in the future */
#if CYC_PER_TICK == 32
      delay = ticks << 5;
#else
      delay = ticks * CYC_PER_TICK;
#endif

      elaps = cycle_count % CYC_PER_TICK;
      if (delay >= elaps)
        {
          delay -= elaps;
        }
    }

  if (delay <= MIN_DELAY)
    {
      val2 = SysTick->VAL;

      SysTick->LOAD = MIN_DELAY - 1;
      SysTick->VAL = 0;

      _TRACE_UNIT('D', 0);
    }
  else
    {
      val2 = SysTick->VAL;

      /* resets timer to last_load */
      SysTick->LOAD = delay - 1;
      SysTick->VAL = 0;

      _TRACE_UNIT('d', delay);
    }

  /*
   * Add elapsed cycles while computing the new load to cycle_count.
   *
   * Note that comparing val1 and val2 is normaly not good enough to
   * guess if the counter wrapped during this interval. Indeed if val1 is
   * close to LOAD, then there are little chances to catch val2 between
   * val1 and LOAD after a wrap. COUNTFLAG should be checked in addition.
   * But since the load computation is faster than MIN_DELAY, then we
   * don't need to worry about this case.
   */
  if (val1 < val2)
    {
      cycle_count += (val1 + (last_load_ - val2));
    }
  else
    {
      cycle_count += (val1 - val2);
    }

  oneshot_running = true;

  return OK;
}

static int rtl876x_systick_oneshot_current(struct oneshot_lowerhalf_s *lower,
                                           clock_t *ticks)
{
  return up_timer_gettick(ticks);
}

static struct oneshot_operations_s systick_ops_priv =
{
  .max_delay                 = rtl876x_systick_oneshot_max_delay,
  .tick_current              = rtl876x_systick_oneshot_current,
};

static struct oneshot_lowerhalf_s systick_priv = {
  .ops = &systick_ops_priv,
};

#ifdef CONFIG_RTL876x_BT
DATA_RAM_FUNCTION
static bool rtl_sys_time_get(uint64_t *p_time_ms)
{
  if (__get_IPSR() != 2)
  {
    irqstate_t flags = enter_critical_section();
    uint64_t cyc = elapsed(NULL) + cycle_count;

#if CYC_PER_TICK == 32
    *p_time_ms = cyc >> 5;
#else
    *p_time_ms = cyc / CYC_PER_TICK;
#endif
    leave_critical_section(flags);
  }
  else
  {
    *p_time_ms = 0;
  }

  return true;
}
#endif

static int rtl_timerisr(int irq, uint32_t *regs, void *arg)
{
  /* Process timer interrupt */
#ifndef CONFIG_SCHED_TICKLESS
  nxsched_process_timer();
#else
  /* Update overflow_cyc and clear COUNTFLAG by invoking elapsed() */
  uint32_t val1, last_load_ = SysTick->LOAD + 1;
  uint32_t elaps = elapsed(&val1);
  uint32_t val2 = SysTick->VAL;
  clock_t ticks;

  /* Update default next cycles to forever */
  SysTick->LOAD = MAX_CYCLES - 1;
  SysTick->VAL = 0;

  cycle_count += elaps;
  overflow_cyc = 0;

  /*
   * Add elapsed cycles while computing the elapsed.
   *
   * Note that comparing val1 and val2 is normaly not good enough to
   * guess if the counter wrapped during this interval. Indeed if val1 is
   * close to LOAD, then there are little chances to catch val2 between
   * val1 and LOAD after a wrap. COUNTFLAG should be checked in addition.
   * But since the load computation is faster than MIN_DELAY, then we
   * don't need to worry about this case.
   */
  if (val1 < val2)
    {
      cycle_count += (val1 + (last_load_ - val2));
    }
  else
    {
      cycle_count += (val1 - val2);
    }

  _TRACE_UNIT('I', cycle_count);

  if (!oneshot_running)
    {
      return 0;
    }

#if CYC_PER_TICK == 32
  ticks = (clock_t)(cycle_count >> 5);
#else
  ticks = (clock_t)(cycle_count / CYC_PER_TICK);
#endif

#ifdef CONFIG_PM
  g_stop_time = ticks;
#endif

  nxsched_alarm_tick_expiration(ticks);
#endif

  return 0;
}

#ifdef CONFIG_PM
#define DATA_RAM_FUNCTION  __attribute__((section(".app.data_ram.text")))

#if defined(CONFIG_RTL876x_BT)
DATA_RAM_FUNCTION
static bool systick_dlps_check_cb(uint32_t * p_value)
{
  uint32_t val1, val2, last_load_ = SysTick->LOAD + 1;
  uint32_t elaps_cycs = elapsed(&val1);
  uint32_t elaps_ticks, unannounced;
  uint64_t total_cycles;
  uint32_t cycs, ticks;
  clock_t now;

  if (val1 <= MIN_DELAY)
    {
      return false;
    }

  FAR struct wdog_s *wdog = (FAR struct wdog_s *)g_wdactivelist.head;
  if (!wdog)
    {
      *p_value = MAX_CYCLES - 1;
      return true;
    }

  total_cycles = cycle_count + elaps_cycs;

#if CYC_PER_TICK == 32
  now = total_cycles >> 5;
  unannounced = total_cycles - (now << 5);
#else
  now = total_cycles / CYC_PER_TICK;
  unannounced = total_cycles - (now * CYC_PER_TICK);
#endif

  elaps_ticks = now - g_stop_time;

  if (wdog->lag <= elaps_ticks + 1)
    {
      return false;
    }

  ticks = 0;

  while (wdog != NULL)
    {
      TIMER_INFO *timer = (TIMER_INFO *)wdog;

      if (timer->work.worker != rtl_timer_callback)
        {
          break;
        }

      if (timer->exclude_magic != EXCLUDE_MAGIC_NUMBER)
        {
          break;
        }

      ticks += wdog->lag;
      wdog = wdog->next;
    }

  if (!wdog)
    {
      *p_value = MAX_CYCLES - 1;
      return true;
    }

  ticks += wdog->lag;
  ticks -= elaps_ticks;

#if CYC_PER_TICK == 32
  cycs = ticks << 5;
#else
  cycs = ticks * CYC_PER_TICK;
#endif

  val2 = SysTick->VAL;

  if (val2 <= MIN_DELAY)
    {
      return false;
    }

  if (val1 < val2)
    {
      unannounced += (val1 + (last_load_ - val2));
    }
  else
    {
      unannounced += (val1 - val2);
    }

  if (cycs < unannounced + MIN_DELAY)
    {
      return false;
    }

  *p_value = cycs - unannounced;

  return true;
}
#else
DATA_RAM_FUNCTION
static bool systick_dlps_check_cb(uint32_t * p_value)
{
  uint32_t val = SysTick->VAL;

  if (val <= MIN_DELAY)
    {
      return false;
    }

  *p_value = val;

  return true;
}
#endif

DATA_RAM_FUNCTION
static void systick_dlps_enter(void)
{
  rtc_enter_dlps_value = __rtl_platform_rtc_get_counter();
  systick_elaps_value  = elapsed(NULL);
}

DATA_RAM_FUNCTION
static void systick_dlps_exit(void)
{
  volatile uint32_t rtc_duing_dlps_value;

  SysTick->LOAD = MAX_CYCLES - 1;
  SysTick->VAL = 0;

  SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);

  rtc_duing_dlps_value = __rtl_platform_rtc_get_counter() - rtc_enter_dlps_value;

  overflow_cyc = systick_elaps_value;
  overflow_cyc += rtc_duing_dlps_value;

  SCB->ICSR |= SCB_ICSR_PENDSTSET_Msk;
}

DATA_RAM_FUNCTION
static void systick_pm_notify(struct pm_callback_s *cb, int domain,
                          enum pm_state_e pmstate)
{
  switch (pmstate)
    {
      case(PM_RESTORE):
        {
          systick_dlps_exit();
        }
        break;

      case(PM_STANDBY):
        {
          systick_dlps_enter();
        }
        break;

      case(PM_SLEEP):
        {
          systick_dlps_enter();
        }
        break;

      default:
        break;
    }
}

#endif

/****************************************************************************
 * Name: up_mdelay
 *
 * Description:
 *   Delay inline for the requested number of milliseconds.
 *   *** NOT multi-tasking friendly ***
 *
 ****************************************************************************/

void up_mdelay(unsigned int milliseconds)
{
  up_udelay(USEC_PER_MSEC * milliseconds);
}

/****************************************************************************
 * Name: up_udelay
 *
 * Description:
 *   Delay inline for the requested number of microseconds.
 *
 *   *** NOT multi-tasking friendly ***
 *
 ****************************************************************************/

void up_udelay(useconds_t microseconds)
{
  __rtl_platform_delay_us(microseconds);
}

void up_timer_initialize(void)
{
  uint32_t regval;

  /* Set the SysTick interrupt to the default priority */

  regval = getreg32(ARMV6M_SYSCON_SHPR3);
  regval &= ~SYSCON_SHPR3_PRI_15_MASK;
  regval |= (NVIC_SYSH_PRIORITY_DEFAULT << SYSCON_SHPR3_PRI_15_SHIFT);
  putreg32(regval, ARMV6M_SYSCON_SHPR3);

  /* Attach the timer interrupt vector */

  irq_attach(NVIC_IRQ_SYSTICK, (xcpt_t)rtl_timerisr, NULL);

#ifndef CONFIG_SCHED_TICKLESS
  SysTick->LOAD = SYSTICK_RELOAD;
#else
  SysTick->LOAD = 0;
#endif

  SysTick->VAL = 0; /* resets timer to last_load */
  SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);

  /* And enable the timer interrupt */
  up_enable_irq(SysTick_VECTORn);
  up_alarm_set_lowerhalf(&systick_priv);

#ifdef CONFIG_PM
  dlps_check_cb_reg((DLPSEnterCheckFunc)systick_dlps_check_cb);
  pm_register(&g_systick_pm);
#endif
}

bool rtl_pm_switch_to_idle(void);

static struct {
  BOOL_PATCH_FUNC p;
} rtl_patch_vector[] __attribute__((section(".patch.dataon.data"))) __attribute__((__used__)) = {
  [7]  = { (BOOL_PATCH_FUNC)rtl_sched_suspend },
  [8]  = { (BOOL_PATCH_FUNC)rtl_sched_resume },
  [9]  = { (BOOL_PATCH_FUNC)rtl_sched_is_start },
  [10] = { (BOOL_PATCH_FUNC)rtl_sched_state_get },
  [18] = { (BOOL_PATCH_FUNC)rtl_task_handle_get },
#if defined(CONFIG_PM)
  [22] = {  (BOOL_PATCH_FUNC)rtl_pm_switch_to_idle },
#endif
  [23] = { (BOOL_PATCH_FUNC)rtl_lock },
  [24] = { (BOOL_PATCH_FUNC)rtl_unlock },
  [25] = { (BOOL_PATCH_FUNC)rtl_sem_create },
  [26] = { (BOOL_PATCH_FUNC)rtl_sem_delete },
  [27] = { (BOOL_PATCH_FUNC)rtl_sem_take },
  [28] = { (BOOL_PATCH_FUNC)rtl_sem_give },
  [29] = { (BOOL_PATCH_FUNC)rtl_mutex_create },
  [30] = { (BOOL_PATCH_FUNC)rtl_mutex_delete },
  [31] = { (BOOL_PATCH_FUNC)rtl_mutex_take },
  [32] = { (BOOL_PATCH_FUNC)rtl_mutex_give },
  [39] = { (BOOL_PATCH_FUNC)rtl_malloc },
  [40] = { (BOOL_PATCH_FUNC)rtl_zalloc },
  [41] = { (BOOL_PATCH_FUNC)rtl_zaligned_alloc },
  [42] = { (BOOL_PATCH_FUNC)rtl_free },
  [43] = { (BOOL_PATCH_FUNC)rtl_free },
#ifdef CONFIG_RTL876x_BT
  [2]  = { (BOOL_PATCH_FUNC)rtl_sys_time_get },
  [13] = { (BOOL_PATCH_FUNC)rtl_task_create },
  [50] = { (BOOL_PATCH_FUNC)rtl_timer_id_get },
  [51] = { (BOOL_PATCH_FUNC)rtl_timer_create },
  [52] = { (BOOL_PATCH_FUNC)rtl_timer_start },
  [53] = { (BOOL_PATCH_FUNC)rtl_timer_restart },
  [54] = { (BOOL_PATCH_FUNC)rtl_timer_stop },
  [55] = { (BOOL_PATCH_FUNC)rtl_timer_delete },
  [57] = { (BOOL_PATCH_FUNC)rtl_time_get_timer_state },
  [59] = { (BOOL_PATCH_FUNC)rtl_timer_number_get },
  [60] = { (BOOL_PATCH_FUNC)rtl_timer_pendcall },
  [61] = { (BOOL_PATCH_FUNC)rtl_timer_next_timeout_value_get },
#endif
};
