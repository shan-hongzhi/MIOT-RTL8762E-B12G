/* Entry Point */
ENTRY(Reset_Handler)
INPUT(mem_define.ld)

MEMORY
{
APP_DATA_ON (rwx)   : ORIGIN = 0x00207C00, LENGTH = APP_GLOBAL_SIZE
HEAP_DATA_ON (rwx)  : ORIGIN = (0x00207C00 + APP_GLOBAL_SIZE) , LENGTH = (65*1024) - APP_GLOBAL_SIZE
CACHE_DATA_ON (rwx)   : ORIGIN = 0x0021C000, LENGTH = SHARE_CACHE_RAM_SIZE
APP_FLASH (r)   : ORIGIN = APP_ADDR, LENGTH = APP_SIZE
APP_TRACE (r): ORIGIN = 0x08800000, LENGTH = 4096k
}

/* Define output sections */
SECTIONS 
{
    . = APP_ADDR;
    app.bin APP_ADDR : 
    {
        KEEP(* (.app.flash.header))
        KEEP(* (.app.flash.header.auth))
    }AT > APP_FLASH
    APP_FLASH_HEADER_EXT  :   
    {
        KEEP(* (.app.flash.header_ext))
    }AT > APP_FLASH

    FLASH_START_ADDR : 
    {
         build/startup_rtl876x_gcc.o(RESET)
         . = ALIGN(4);
    __flash_start_load_ad__ = LOADADDR(FLASH_START_ADDR);
    __flash_start_exe_ad__ = ADDR(FLASH_START_ADDR);
    }AT > APP_FLASH
    
    FLASH_TEXT  :
    {
        *(EXCLUDE_FILE (build/swap.o build/swap_helper.o build/cortex_m_systick.o build/timeout.o) .text*)
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);
        *(.app.flash.text)
        *(.app.flash.rodata)
    }AT > APP_FLASH
    RAM_VECTOR_TABLE 0x00200000: 
    {
        __ram_vector_table_start__ = .;
        KEEP(*(VECTOR))
        __ram_vector_table_end__ = .;
    }AT > APP_FLASH
    __ram_vector_load_ad__ = LOADADDR(RAM_VECTOR_TABLE);
    __ram_vector_table_length__ = __ram_vector_table_end__ - __ram_vector_table_start__; 
    RAM_DATA_ON  :
    {
        __ram_dataon_ro_start__ = .;
        * (.app.data_ram.text)
        build/swap.o (.text*)
        build/swap_helper.o (.text*)
        build/cortex_m_systick.o (.text*)
        build/timeout.o (.text*)
        . = ALIGN(4);
        __ram_dataon_ro_end__ = .;
        __ram_dataon_rw_start__ = .;
        *(.data*)
        * (.ram.dataon.data)
        __ram_dataon_rw_end__ = .;
        . = ALIGN(4);
    } > APP_DATA_ON  AT > APP_FLASH
    BSS (NOLOAD):
    {
        . = ALIGN(4);
        __ram_dataon_zi_start__ = .;
        * (.ram.dataon.bss*)
        *(.bss*)
        *(COMMON)
        *(._k_stack.static.async_msg_free)
        . = ALIGN(4);
        __ram_dataon_zi_end__ = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __ram_ro_load_ad__ = LOADADDR(RAM_DATA_ON);
    __ram_dataon_ro_length__ = __ram_dataon_ro_end__ - __ram_dataon_ro_start__;
    __ram_dataon_rw_length__ = __ram_dataon_rw_end__ - __ram_dataon_rw_start__;
    __ram_dataon_zi_length__ = __ram_dataon_zi_end__ - __ram_dataon_zi_start__;
    __ram_rw_load_ad__ = LOADADDR(RAM_DATA_ON)+__ram_dataon_ro_length__;
    initlevel :
    {
        . = ALIGN(4);
        __init_start = .;
        __init_PRE_KERNEL_1_start = .;
        KEEP(*(SORT(.z_init_PRE_KERNEL_1[0-9]_*)));
        KEEP(*(SORT(.z_init_PRE_KERNEL_1[1-9][0-9]_*)));
        __init_PRE_KERNEL_2_start = .;
        KEEP(*(SORT(.z_init_PRE_KERNEL_2[0-9]_*)));
        KEEP(*(SORT(.z_init_PRE_KERNEL_2[1-9][0-9]_*)));
        __init_POST_KERNEL_start = .;
        KEEP(*(SORT(.z_init_POST_KERNEL_[0-9]*)));
        KEEP(*(SORT(.z_init_POST_KERNEL_[1-9][0-9]*)));
        __init_APPLICATION_start = .;
        KEEP(*(SORT(.z_init_APPLICATION_[0-9]*)));
        KEEP(*(SORT(.z_init_APPLICATION_[1-9][0-9]*)));
        __init_SMP_start = .;
        KEEP(*(SORT(.z_init_SMP_[0-9]*)));
        KEEP(*(SORT(.z_init_SMP_[1-9][0-9]*)));
        __init_end = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __init_load_ad__ = LOADADDR(initlevel);
    __init_length = __init_end - __init_start;
    devices :
    {
        . = ALIGN(4);
        __device_start = .;
        __device_PRE_KERNEL_1_start = .;
        KEEP(*(SORT(.z_device_PRE_KERNEL_1[0-9]_*)));
        KEEP(*(SORT(.z_device_PRE_KERNEL_1[1-9][0-9]_*)));
        __device_PRE_KERNEL_2_start = .;
        KEEP(*(SORT(.z_device_PRE_KERNEL_2[0-9]_*)));
        KEEP(*(SORT(.z_device_PRE_KERNEL_2[1-9][0-9]_*)));
        __device_POST_KERNEL_start = .;
        KEEP(*(SORT(.z_device_POST_KERNEL_[0-9]*)));
        KEEP(*(SORT(.z_device_POST_KERNEL_[1-9][0-9]*)));
        __device_APPLICATION_start = .;
        KEEP(*(SORT(.z_device_APPLICATION_[0-9]*)));
        KEEP(*(SORT(.z_device_APPLICATION_[1-9][0-9]*)));
        __device_SMP_start = .;
        KEEP(*(SORT(.z_device_SMP_[0-9]*)));
        KEEP(*(SORT(.z_device_SMP_[1-9][0-9]*)));
        __device_end = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __device_load_ad__ = LOADADDR(devices);
    __device_length = __device_end - __device_start;
    noinit (NOLOAD):
    {
        *(.noinit)
        *(.noinit.*)
        *(.kernel_noinit.*)
    } > APP_DATA_ON  AT > APP_FLASH
    _static_thread_data_area ALIGN(4):
    {
        __static_thread_data_list_start = .;
        KEEP(*(SORT_BY_NAME(.__static_thread_data.static.*)));
        __static_thread_data_list_end = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __static_thread_data_load_ad__ = LOADADDR(_static_thread_data_area);
    __static_thread_data_length = __static_thread_data_list_end - __static_thread_data_list_start;
    _k_heap_area ALIGN(4):
    {
        _k_heap_list_start = .;
        *(SORT_BY_NAME(._k_heap.static.*));
        _k_heap_list_end = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __k_heap_load_ad__ = LOADADDR(_k_heap_area);
    __k_heap_length = _k_heap_list_end - _k_heap_list_start;
    _k_mem_slab_area ALIGN(4):
    {
        _k_mem_slab_list_start = .;
        *(SORT_BY_NAME(._k_mem_slab.static.*));
        _k_mem_slab_list_end = .;
    } > APP_DATA_ON  AT > APP_FLASH
    __k_mem_slab_load_ad__ = LOADADDR(_k_mem_slab_area);
    __k_mem_slab_length = _k_mem_slab_list_end - _k_mem_slab_list_start;
    HEAP_DATA_ON  :
    {
        __heap_start__ = .;
        end = __heap_start__;
        _end = end;
        __end = end;
        KEEP(*(.heap))
        __heap_end__ = .;
        __HeapLimit = __heap_end__;
    } > HEAP_DATA_ON  AT > APP_FLASH

    CACHE_DATA_ON (OVERLAY):  
    {   
        __cache_dataon_start__ = . ;
        *(.ram.sharecacheram.text)
        __cache_dataon_end__ = .;
    }>CACHE_DATA_ON AT> APP_FLASH
    __cache_dataon_length__ = __cache_dataon_end__ - __cache_dataon_start__;
    __cache_data_load_ad__ = LOADADDR(RAM_DATA_ON);
    App.trace 0x08800000: 
    {
        *(.TRACE*)
    }>APP_TRACE

}

