/* Entry Point */
INPUT(romsym_is.ld)
INPUT(mem_define.ld)

MEMORY
{
  APP_DATA_ON (rwx)   : ORIGIN = APP_GLOBAL_ADDR, LENGTH = APP_GLOBAL_SIZE
  BUF_DATA_ON (rwx)   : ORIGIN = RTL_BUF_ADDR, LENGTH = RTL_BUF_SIZE
  PATCH_DATA (rwx)    : ORIGIN = RTL_PATCH_ADDR, LENGTH = RTL_PATCH_SIZE
  APP_FLASH (r)     : ORIGIN = APP_ADDR, LENGTH = APP_SIZE
  APP_TRACE (r)     : ORIGIN = 0x08800000, LENGTH = 4096k
}

/* Define output sections */
SECTIONS
{
  . = APP_ADDR;
  app.bin APP_ADDR :
  {
    __app_flash_start__ = .;
    KEEP(*(.app.flash.header))
    KEEP(*(.app.flash.header.auth))
  } AT > APP_FLASH

  APP_FLASH_HEADER_EXT  :
  {
    KEEP(*(.app.flash.header_ext))
  } AT > APP_FLASH

  FLASH_START_ADDR :
  {
    startup_rtl876x_gcc.o(RESET)
    . = ALIGN(4);
    __flash_start_load_ad__ = LOADADDR(FLASH_START_ADDR);
    __flash_start_exe_ad__ = ADDR(FLASH_START_ADDR);
  } AT > APP_FLASH

  RAM_CODE :
  {
    _stextram = ABSOLUTE(.);
    __ram_text_start__ = .;

    __ram_first_stage_text_start__ = .;
    *(.vectors)
    *(.app.first_stage_ram.text)
    . = ALIGN(4);
    __ram_first_stage_text_end__ = .;

    __ram_second_stage_text_start__ = .;
    *(.app.data_ram.text)

    . = ALIGN(4);
    __dlps_dataon_start__ = .;
    KEEP(*(.dlps.dataon.data))
    __dlps_dataon_end__ = .;

    *libc.a:dq_remfirst.o(.text .text.*)
    *libc.a:dq_remlast.o(.text .text.*)
    *libc.a:sq_remfirst.o(.text .text.*)
    *libc.a:sq_remafter.o(.text .text.*)
    *libc.a:lib_memcmp.o(.text .text.*)
    *libc.a:lib_memset.o(.text .text.*)
    *libc.a:lib_memcpy.o(.text .text.*)
    *libc.a:lib_strlen.o(.text .text.*)
    *libc.a:lib_strcpy.o(.text .text.*)
    *libc.a:lib_strcmp.o(.text .text.*)
    *libc.a:lib_strncmp.o(.text .text.*)

    *libsched.a:sched_processtimer.o(.text .text.*)
    *libsched.a:sched_lock.o(.text .text.*)
    *libsched.a:sched_unlock.o(.text .text.*)
    *libsched.a:irq_dispatch.o(.text .text.*)
    *libsched.a:wd_start.o(.text .text.wd_timer)
    *libsched.a:sched_timerexpiration.o(.text .text.*)
    *libsched.a:sem_wait.o(.text .text.*)
    *libsched.a:sem_post.o(.text .text.*)
    *libsched.a:sem_trywait.o(.text .text.*)

    *libarch.a:arm_exception.o(.text .text.*)
    *libarch.a:arm_doirq.o(.text .text.*)
    *libarch.a:arm_switchcontext.o(.text .text.*)
    *libarch.a:arm_hardfault.o(.text .text.*)
    *libarch.a:arm_svcall.o(.text .text.*)
    *libarch.a:rtl876x_irq.o(.text .text.__arm_hardfault)
    *libarch.a:rtl876x_irq.o(.text .text.arm_ack_irq)
    *libarch.a:rtl876x_os.o(.text .text.up_mdelay)
    *libarch.a:rtl876x_os.o(.text .text.up_udelay)
    *libarch.a:rtl876x_os.o(.text .text.rtl_timerisr)
    *libarch.a:rtl876x_os.o(.text .text.elapsed)

    *libdrivers.a:pm_checkstate.o(.text .text.pm_checkstate)
    *libdrivers.a:pm_changestate.o(.text .text.*)
    *libdrivers.a:pm_lock.o(.text .text.*)

    *libarch.a:rtl876x_os.o(.text .text.rtl_timer_callback)
    *libarch.a:rtl876x_os.o(.text .text.work_pending_cb)
    *libarch.a:rtl876x_os.o(.text .text.rtl_timer_start)
    *libarch.a:rtl876x_os.o(.text .text.rtl_timer_restart)
    *libarch.a:rtl876x_os.o(.text .text.rtl_timer_stop)

    *libarch.a:rtl876x_os.o(.text .text.up_timer_gettick)
    *libarch.a:rtl876x_os.o(.text .text.tick_left)
    *libarch.a:rtl876x_os.o(.text .text.up_alarm_tick_start)
    *libarch.a:rtl876x_os.o(.text .text.up_alarm_tick_cancel)

    *libarch.a:rtl_hci.o(.text .text.hci_if_callback)
    *libarch.a:hci_virtual.o(.text .text.hci_if_confirm)

    *libsched.a:sched_idletask.o(.text .text.*)
    *libsched.a:nx_start.o(.rodata .rodata.g_tasklisttable)
    *libsched.a:sched_addreadytorun.o(.text .text.*)
    *libsched.a:sched_removereadytorun.o(.text .text.*)
    *libsched.a:sched_mergepending.o(.text .text.*)
    *libsched.a:sched_addprioritized.o(.text .text.*)

    /* Copy some zephyr ble functions to ram_code to improve the
     * performance.
     */

    *libzblue.a:*.o(.text.work_queue_main)
    *libzblue.a:*.o(.text.work_timeout)
    *libzblue.a:*.o(.text.z_sched_wait)

    *libzblue.a:*.o(.text.bt_vadapter_recv)
    *libzblue.a:*.o(.text.rx_thread)
    *libzblue.a:*.o(.text.net_buf_get)
    *libzblue.a:*.o(.text.bt_buf_get_evt)
    *libzblue.a:*.o(.text.bt_buf_get_cmd_complete)
    *libzblue.a:*.o(.text.net_buf_ref)
    *libzblue.a:*.o(.text.net_buf_simple_reserve)
    *libzblue.a:*.o(.text.net_buf_alloc_fixed)
    *libzblue.a:*.o(.text.net_buf_alloc_len)
    *libzblue.a:*.o(.text.net_buf_add_u8)
    *libzblue.a:*.o(.text.net_buf_add_mem)
    *libzblue.a:*.o(.text.bt_recv)
    *libzblue.a:*.o(.text.rx_queue_put)
    *libzblue.a:*.o(.text.rx_work_handler)
    *libzblue.a:*.o(.text.net_buf_slist_get)
    *libzblue.a:*.o(.text.net_buf_slist_put)
    *libzblue.a:*.o(.text.net_buf_pull_u8)
    *libzblue.a:*.o(.text.net_buf_pull_mem)
    *libzblue.a:*.o(.text.net_buf_pull)
    *libzblue.a:*.o(.text.bt_hci_evt_get_flags)
    *libzblue.a:*.o(.text.net_buf_simple_save)
    *libzblue.a:*.o(.text.net_buf_simple_restore)

    *libzblue.a:*.o(.text.flag_test_and_clear)
    *libzblue.a:*.o(.text.sys_slist_append)
    *libzblue.a:*.o(.text.sys_put_be32)
    *libzblue.a:*.o(.text.sys_put_le32)
    *libzblue.a:*.o(.text.atomic_test_bit)
    *libzblue.a:*.o(.text.atomic_set_bit_to)
    *libzblue.a:*.o(.text.atomic_set_bit_to.*)
    *libzblue.a:*.o(.text.sys_slist_get)
    *libzblue.a:*.o(.text.sys_slist_find_and_remove)
    *libzblue.a:*.o(.text.sys_slist_find_and_remove.*)
    *libzblue.a:*.o(.text.sys_get_be24)
    *libzblue.a:*.o(.text.z_tick_get)

    *libzblue.a:*.o(.text.atomic_and)
    *libzblue.a:*.o(.text.atomic_get)
    *libzblue.a:*.o(.text.atomic_or)
    *libzblue.a:*.o(.text.atomic_set)
    *libzblue.a:*.o(.text.atomic_sub)

    *libzblue.a:*.o(.text.arch_irq_lock)
    *libzblue.a:*.o(.text.arch_irq_unlock)

    *libzblue.a:*.o(.text.k_sem_give)
    *libzblue.a:*.o(.text.k_sem_take)

    *libzblue.a:*.o(.text.k_queue_append)
    *libzblue.a:*.o(.text.z_handle_obj_poll_events)
    *libzblue.a:*.o(.text.k_poll)
    *libzblue.a:*.o(.text.k_queue_get)
    *libzblue.a:*.o(.text.k_queue_poll)
    *libzblue.a:*.o(.text.k_sched_lock)
    *libzblue.a:*.o(.text.k_sched_unlock)

    *libzblue.a:*.o(.text.net_buf_simple_add)
    *libzblue.a:*.o(.text.net_buf_simple_pull)

    . = ALIGN(4);
    __ram_second_stage_text_end__ = .;
    __ram_text_end__ = .;
    _etextram = ABSOLUTE(.);
  } > APP_DATA_ON  AT > APP_FLASH

  __ram_text_load_ad__ = LOADADDR(RAM_CODE);

  __ram_first_stage_text_load_ad__ = __ram_text_load_ad__;
  __ram_first_stage_text_length__ = __ram_first_stage_text_end__ - __ram_first_stage_text_start__;

  __ram_second_stage_text_load_ad__ = __ram_first_stage_text_load_ad__ + __ram_first_stage_text_length__;
  __ram_second_stage_text_length__ = __ram_second_stage_text_end__ - __ram_second_stage_text_start__;

  __ram_text_length__ = __ram_text_end__ - __ram_text_start__;


  FLASH_TEXT :
  {
    _stext = ABSOLUTE(.);
    *(.text*)
    . = ALIGN(4);
    *(.rodata*)
    . = ALIGN(4);
    *(.app.flash.text)
    *(.app.flash.rodata)
    _etext = ABSOLUTE(.);
  } AT > APP_FLASH

  .ARM.extab :
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } AT >APP_FLASH

  .ARM.exidx :
  {
    . = ALIGN(8);
    __exidx_start = ABSOLUTE(.);
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = ABSOLUTE(.);
    . = ALIGN(8);
  } AT >APP_FLASH

  RAM_DATA_ON  :
  {
    . = ALIGN(4);
    __ram_dataon_ro_start__ = .;
    __ram_dataon_ro_end__ = .;

    . = ALIGN(4);
    __ram_dataon_rw_start__ = .;
    *(.data*)
    *(.ram.dataon.data)
    . = ALIGN(4);
    __ram_dataon_rw_end__ = .;
  } > APP_DATA_ON  AT > APP_FLASH

  OVERLAY : NOCROSSREFS
  {
    .ov1 {*libmesh_auth_vela_rtl8762e.a:*.o(.bss.mible_auth) }
    .ov2 {*libmesh_auth_vela_rtl8762e.a:*.o(.bss.mible_dfu) }
  } > APP_DATA_ON  AT > APP_FLASH

  .bss (NOLOAD):
  {
    . = ALIGN(4);
    __ram_dataon_zi_start__ = .;
    *(.ram.dataon.bss*)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __ram_dataon_zi_end__ = .;
    _ebss = ABSOLUTE(.);
  } > APP_DATA_ON  AT > APP_FLASH

  __ram_ro_load_ad__ = LOADADDR(RAM_DATA_ON);
  __ram_dataon_ro_length__ = __ram_dataon_ro_end__ - __ram_dataon_ro_start__;
  __ram_dataon_rw_length__ = __ram_dataon_rw_end__ - __ram_dataon_rw_start__;
  __ram_dataon_zi_length__ = __ram_dataon_zi_end__ - __ram_dataon_zi_start__;
  __ram_rw_load_ad__ = LOADADDR(RAM_DATA_ON)+__ram_dataon_ro_length__;

  BUF_DATA :
  {
    . = ALIGN(4);
    __buf_dataon_rw_start__ = .;
    *(.buf.dataon.data)
    . = ALIGN(4);
    __buf_dataon_rw_end__ = .;
  } > BUF_DATA_ON  AT > APP_FLASH

  __buf_rw_load_ad__ = LOADADDR(BUF_DATA);
  __buf_dataon_rw_length__ = __buf_dataon_rw_end__ - __buf_dataon_rw_start__;

  BUF_BSS (NOLOAD):
  {
    . = ALIGN(4);
    __buf_dataon_bss_rw_start__ = .;
    *(.buf.dataon.bss)
    . = ALIGN(4);
    __buf_dataon_bss_rw_end__ = .;
  } > BUF_DATA_ON  AT > APP_FLASH

  __buf_dataon_bss_length__ = __buf_dataon_bss_rw_end__ - __buf_dataon_bss_rw_start__;
  __buf_dataon_length__ = __buf_dataon_bss_rw_end__ - __buf_dataon_rw_start__;

  PATCH :
  {
    . = ALIGN(4);
    __patch_rw_start__ = .;
    KEEP(*(.patch.dataon.data))
    . = ALIGN(4);
    __patch_rw_end__ = .;
  } > PATCH_DATA AT > APP_FLASH

  __patch_rw_load_ad__ = LOADADDR(PATCH);
  __patch_rw_length__ = __patch_rw_end__ - __patch_rw_start__;

  APP_CFG_INFO  :
  {
    KEEP(*(.cfg))
    . = ALIGN(4);
  }AT > APP_FLASH

  APP_VERSION_INFO  :
  {
    KEEP(*(.info))
    . = ALIGN(4);
    __app_flash_end__ = .;
  }AT > APP_FLASH

  App.trace 0x08800000:
  {
    *(.TRACE*)
  } > APP_TRACE
}
