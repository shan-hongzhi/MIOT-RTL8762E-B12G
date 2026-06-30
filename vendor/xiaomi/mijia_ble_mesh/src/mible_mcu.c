
#include "mible_mcu.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "mible_dfu.h"
#include <stdio.h>
#include "xmodem.h"
#include <errno.h>
#include "sys_clock.h"
#include <nuttx/serial/tioctl.h>
#include <termios.h>
#include <sys/ioctl.h>

#if defined(CONFIG_MIBLE_USE_MCU_OTA)

#if defined(MI_LOG_ENABLED) && (MI_LOG_ENABLED)
#include <sys/printk.h>
#else
#undef printk
#define printk(fmt, ...)
#endif

static int fd_mcu_uart;
static struct
{
    miio_xmodem_t xmodem;
    void *data;
    uint32_t length;
} mijia_mcu_ota;

static const struct
{
    uint8_t req_len;
    uint32_t timeout;
} mible_mcu_req_table[MIBLE_MCU_TRANSFER] =
{
        [MIBLE_MCU_GET_VERSION] = {5, 100},
        [MIBLE_MCU_READ_DFUINFO] = {26, 200},
        [MIBLE_MCU_WRITE_DFUINFO] = {3, 200},
        [MIBLE_MCU_WRITE_FIRMWARE] = {3, 200},
        [MIBLE_MCU_VERIFY_FIRMWARE] = {3, 3000},
        [MIBLE_MCU_UPGRADE_FIRMWARE] = {3, 100},
};

extern const volatile struct mible_config mi_config;
/****************************************************************************
 * Name: mible_mcu_cmd_send
 *
 * Description:
 *   Function for write cmd to slave.
 *
 * Input Parameters:
 *   cmd  - wait for data from mcu at cmd.
 *   arg  - buffer pt for return.
 *
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_cmd_send(mible_mcu_cmd_t cmd, void *arg)
{
    mible_status_t ret = MI_SUCCESS;
    unsigned int last_index = 0;
    unsigned int crc32 = 0;
    unsigned int recv_bytes = 0;
    unsigned char *buf = mijia_mcu_ota.xmodem.xbuff; /* borrow xmodem */

    //flush iobuffer
    mible_mcu_flushinput();
    
    switch(cmd) {
    case MIBLE_MCU_GET_VERSION:
        printk("down get_ver\r");
        ret = mible_mcu_send_buffer("down get_ver\r", strlen("down get_ver\r"));
        break;
    case MIBLE_MCU_READ_DFUINFO:
        printk("down rd_info\r");
        ret = mible_mcu_send_buffer("down rd_info\r", strlen("down dfu_rdinfo\r"));
        break;
    case MIBLE_MCU_WRITE_DFUINFO:
        last_index = ((mible_dfu_info_t *)arg)->last_index;
        crc32 = ((mible_dfu_info_t *)arg)->crc32;
        recv_bytes = ((mible_dfu_info_t *)arg)->recv_bytes;
        sprintf((char *)buf, "down wr_info %d %08x %d\r", last_index, crc32, recv_bytes);
        printk("%s", buf);
        ret = mible_mcu_send_buffer(buf, strlen((char *)buf));
        if(MI_SUCCESS != ret){
            printk("MIBLE_MCU_WRITE_DFUINFO failed\n");
        }
        break;
    case MIBLE_MCU_WRITE_FIRMWARE:
        sprintf((char *)buf, "down nvm_write %08x\r", (unsigned int)(((mible_mcu_nvminfo_t *)arg)->address));
        printk("%s", buf);
        mible_mcu_send_buffer(buf, strlen((char *)buf));        
        break;
    case MIBLE_MCU_VERIFY_FIRMWARE:
        printk("down dfu_verify\r");
        ret = mible_mcu_send_buffer("down dfu_verify\r", strlen("down dfu_verify\r"));
        break;
    case MIBLE_MCU_UPGRADE_FIRMWARE:
        printk("down dfu_active\r");
        ret = mible_mcu_send_buffer("down dfu_active\r", strlen("down dfu_active\r"));        
        break;
    case MIBLE_MCU_TRANSFER:
        if (MI_SUCCESS != miio_xmodem_create_instance(&mijia_mcu_ota.xmodem))
        {
            printk("miio_xmodem_create_instance FAIL!!\n");
            ret = MI_ERR_NO_MEM;
            break;
        }

        printk("MIBLE_MCU_TRANSFER addr %08lx, len %ld\n",
                     ((mible_mcu_nvminfo_t *)arg)->address, ((mible_mcu_nvminfo_t *)arg)->length);
        
        mijia_mcu_ota.data = ((mible_mcu_nvminfo_t *)arg)->p_data;
        mijia_mcu_ota.length = ((mible_mcu_nvminfo_t *)arg)->length;
        
        break;
    default:
        break;
    }
    return ret;
}
/****************************************************************************
 * Name: mible_mcu_cmd_wait
 *
 * Description:
 *   Function for read uart data from slave or send xmodem data to slave.
 *
 * Input Parameters:
 *   cmd  - wait for data from mcu at cmd.
 *   arg  - buffer pt for return.
 *
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_cmd_wait(mible_mcu_cmd_t cmd, void *arg)
{
    uint8_t *recv = mijia_mcu_ota.xmodem.xbuff; /* borrow xmodem */
    uint8_t read_cnt;
    /* add your own code here*/
    mible_status_t ret = MI_ERR_BUSY;
    unsigned int last_index = 0;
    unsigned int crc32 = 0;
    unsigned int recv_bytes = 0;
    char status;

    if (cmd == MIBLE_MCU_TRANSFER)
    {
        status = xmodem_transfer_data_pt(&mijia_mcu_ota.xmodem, mijia_mcu_ota.data, mijia_mcu_ota.length);
        if (status < 0)
        {
            ret = MI_ERR_INTERNAL;
            printk("xmodem_transfer_data failed\n");
        }
        else
        {
            printk("xmodem_transfer_data successful\n");
            ret = MI_SUCCESS;
        }
        return ret;
    }

    ret = mible_mcu_recv_bytes(recv, mible_mcu_req_table[cmd].req_len,mible_mcu_req_table[cmd].timeout);
    
    switch(cmd) {
    case MIBLE_MCU_GET_VERSION:
        for (read_cnt = 0; read_cnt < 4; read_cnt++)
        {
            if (recv[read_cnt] < '0' || recv[read_cnt] > '9')
            {
                printk("MIBLE_MCU_GET_VERSION invalid formate!!!\n");
                ret = MI_ERR_INVALID_PARAM;
            }
        }
        memcpy((uint8_t *)arg, recv, mible_mcu_req_table[cmd].req_len);
        break;
    case MIBLE_MCU_READ_DFUINFO:
        if (0 == strncmp((const char *)recv, "ok", strlen("ok")))
        {
            read_cnt = sscanf((const char *)recv, "%*s%04x%08x%08x", &last_index, &crc32, &recv_bytes);
            if (read_cnt == 3)
            {
                printk("mible_mcu_read_dfuinfo succ index %d, cec32 %08x, recv %d!!!\n",
                                last_index, crc32, recv_bytes);
                ((mible_dfu_info_t *)arg)->last_index = last_index;
                ((mible_dfu_info_t *)arg)->crc32 = crc32;
                ((mible_dfu_info_t *)arg)->recv_bytes = recv_bytes;
            }
            else
            {
                printk("mible_mcu_read_dfuinfo fail : param %d!!!\n", read_cnt);
                ret = MI_ERR_INVALID_PARAM;
            }
        }
        else
        {
            printk("mible_mcu_read_dfuinfo fail !!!\n");
            ret = MI_ERR_NOT_FOUND;
        }
        break;
    case MIBLE_MCU_WRITE_DFUINFO:
        if (0 == strncmp((const char *)recv, "ok", strlen("ok")))
        {
            printk("mible_mcu_write_dfuinfo succ !!!\n");
        }
        else
        {
            printk("mible_mcu_write_dfuinfo fail !!!\n");
            ret = MI_ERR_NOT_FOUND;
        }
        break;
    case MIBLE_MCU_WRITE_FIRMWARE:
        if (0 == strncmp((const char *)recv, "ok", strlen("ok")))
        {
            printk("receive ok, start xmodem!!!\n");
        }
        else
        {
            printk("mible_mcu_nvm_write fail !!!\n");
            ret = MI_ERR_NOT_FOUND;
        }
        break;
    case MIBLE_MCU_VERIFY_FIRMWARE:
        if (0 == strncmp((const char *)recv, "ok", strlen("ok")))
        {
            printk("mible_mcu_verify_firmware succ !!!\n");
        }
        else
        {
            printk("mible_mcu_verify_firmware fail !!!\n");
            ret = MI_ERR_NOT_FOUND;
        }
        break;
    case MIBLE_MCU_UPGRADE_FIRMWARE:
        if (0 == strncmp((const char *)recv, "ok", strlen("ok")))
        {
            printk("mible_mcu_upgrade_firmware succ !!!\n");
        }
        else
        {
            printk("mible_mcu_upgrade_firmware fail !!!\n");
            ret = MI_ERR_NOT_FOUND;
        }        
        break;
    default:
        break;
    }
    return ret;
}
/****************************************************************************
 * Name: mible_mcu_send_get_info
 *
 * Description:
 *   Function for send get mcu version cmd from slave.
 *
 * Input Parameters:
 *   *delay  - version cmd delay time.
 *
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_send_get_info(uint32_t *delay)
{
    mible_status_t ret;

    ret = mible_mcu_cmd_send(MIBLE_MCU_GET_VERSION, NULL);
    if(MI_SUCCESS != ret)
        return MI_ERR_INTERNAL;

    *delay = mible_mcu_req_table[MIBLE_MCU_GET_VERSION].timeout;

    return MI_SUCCESS;
}
/****************************************************************************
 * Name: mible_mcu_read_get_info
 *
 * Description:
 *   Function for send get mcu version cmd from slave.
 *
 * Input Parameters:
 *   buf  - mcu version buffer pt.
 *
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_read_get_info(uint8_t *buf)
{
    mible_status_t ret;

    ret = mible_mcu_cmd_wait(MIBLE_MCU_GET_VERSION, (void *)buf);
    printk("%02x %02x %02x %02x\n", (uint8_t )*buf,(uint8_t ) *(buf+1),(uint8_t ) *(buf+2),(uint8_t ) *(buf+3));
    if(MI_SUCCESS != ret) {
        printk("mible_mcu_get_info fail %d\n", ret);
        return MI_ERR_NOT_FOUND;
    }

    uint16_t mcu_ver_temp = 0;
    mcu_ver_temp += ((uint8_t )*buf - '0')*1000;
    mcu_ver_temp += ((uint8_t )*(buf+1) - '0')*100;
    mcu_ver_temp += ((uint8_t )*(buf+2) - '0')*10;
    mcu_ver_temp += ((uint8_t )*(buf+3) - '0');
    mi_config.cfg.mcu_version[0] = (uint8_t)(mcu_ver_temp&0xff);
    mi_config.cfg.mcu_version[1] = (uint8_t)((mcu_ver_temp>>8)&0xff);

    return MI_SUCCESS;
}

/****************************************************************************
 * Name: mible_mcu_read_dfuinfo
 *
 * Description:
 *   Function for send read dfu info cmd from slave.
 *
 * Input Parameters:
 *   m_dfu_info  - nvm data pt.
 *   len - sizeof dfu info .
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
void mible_mcu_read_dfuinfo(uint8_t *m_dfu_info, uint8_t len)
{
    mible_status_t ret;

    ret = mible_mcu_cmd_send(MIBLE_MCU_READ_DFUINFO, NULL);
    if (MI_SUCCESS != ret)
        return;
    
    ret = mible_mcu_cmd_wait(MIBLE_MCU_READ_DFUINFO, (void *)m_dfu_info);
    if (MI_SUCCESS != ret){
        printk("mible_mcu_read_dfuinfo fail %d\n", ret);
        return;
    }
}
/****************************************************************************
 * Name: mible_mcu_write_dfuinfo
 *
 * Description:
 *   Function for send write dfu info cmd to slave.
 *
 * Input Parameters:
 *   m_dfu_info  - nvm data pt.
 *   len - sizeof dfu info .
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_write_dfuinfo(uint8_t *m_dfu_info, uint8_t len)
{
    mible_status_t ret;
    
    ret = mible_mcu_cmd_send(MIBLE_MCU_WRITE_DFUINFO, (void *)m_dfu_info);
    if (MI_SUCCESS != ret)
        return MI_ERR_INTERNAL;
    
    ret = mible_mcu_cmd_wait(MIBLE_MCU_WRITE_DFUINFO, NULL);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }

    return MI_SUCCESS;
}
/****************************************************************************
 * Name: mible_mcu_upgrade_firmware
 *
 * Description:
 *   Function for send switch ota bank cmd to slave.
 *
 * Input Parameters:
 *   void  
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_upgrade_firmware(void)
{
    mible_status_t ret;
    
    ret = mible_mcu_cmd_send(MIBLE_MCU_UPGRADE_FIRMWARE, NULL);
    if (MI_SUCCESS != ret)
        return MI_ERR_INTERNAL;

    ret = mible_mcu_cmd_wait(MIBLE_MCU_UPGRADE_FIRMWARE, NULL);
    if(MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }

    return MI_SUCCESS;
}
/****************************************************************************
 * Name: mible_mcu_verify_firmware
 *
 * Description:
 *   Function for send verify cert cmd to slave.
 *
 * Input Parameters:
 *   void  
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_verify_firmware(void)
{
    mible_status_t ret;
    
    ret = mible_mcu_cmd_send(MIBLE_MCU_VERIFY_FIRMWARE, NULL);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }
    ret = mible_mcu_cmd_wait(MIBLE_MCU_VERIFY_FIRMWARE, NULL);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }
    return MI_SUCCESS;
}
/****************************************************************************
 * Name: mible_mcu_nvm_write
 *
 * Description:
 *   Function for send nvm data to slave.
 *
 * Input Parameters:
 *   p_data  - nvm data pt.
 *   length - nvm data length.
 *   address - flash write addr.
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 ****************************************************************************/
mible_status_t mible_mcu_nvm_write(void *p_data, uint32_t length, uint32_t address)
{
    mible_status_t ret = MI_SUCCESS;
    mible_mcu_nvminfo_t nvminfo = {
        .address = address,
        .length = length,
        .p_data = p_data
    };
    
    printk("mible_mcu_nvm_write addr %08lu, len %lu\n", nvminfo.address, nvminfo.length);
    ret = mible_mcu_cmd_send(MIBLE_MCU_WRITE_FIRMWARE, (void *)&nvminfo);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }
    ret = mible_mcu_cmd_wait(MIBLE_MCU_WRITE_FIRMWARE, NULL);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }
    /*start xmodem*/
    ret = mible_mcu_cmd_send(MIBLE_MCU_TRANSFER, (void *)&nvminfo);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }
    ret = mible_mcu_cmd_wait(MIBLE_MCU_TRANSFER, NULL);
    if (MI_SUCCESS != ret){
        return MI_ERR_INTERNAL;
    }

    return MI_SUCCESS;
}
/****************************************************************************
 * Name: mible_mcu_init
 *
 * Description:
 *   Function for open fd.
 *
 * Input Parameters:
 *   void.
 * Returned Value:
 *   Upon successful completion, fd is returned. Otherwise, 0 is returned
 *
 ****************************************************************************/
int mible_mcu_init(void)
{
#if defined(CONFIG_RTL876x_UART1)
    fd_mcu_uart = open("/dev/ttyS1", O_RDWR|O_NONBLOCK);
    if (fd_mcu_uart < 0) {
        printk("%s open failed\n", "/dev/ttyS1");
        return fd_mcu_uart;
    }
    return 0;
#else
    return -1;
#endif
}
/****************************************************************************
 * Name: mible_mcu_deinit
 *
 * Description:
 *   Function for close fd.
 *
 * Input Parameters:
 *   void.
 * Returned Value:
 *   void.
 *
 ****************************************************************************/
void mible_mcu_deinit(void)
{
    close(fd_mcu_uart);
}
/****************************************************************************
 * Name: mible_mcu_send_buffer
 *
 * Description:
 *   Function for send one byte to uart fd.
 *
 * Input Parameters:
 *   pSend_Buf  - send buffer pt.
 *   vCount  -  send count
 * Returned Value:
 *   MI_SUCCESS or MI_ERR_INTERNAL.
 *
 ****************************************************************************/
mible_status_t mible_mcu_send_buffer(const uint8_t *pSend_Buf, uint16_t vCount)
{
    rsize_t send_count;
    mible_status_t ret = MI_SUCCESS;
    
    send_count = mible_mcu_uart_send_data((uint8_t *)pSend_Buf,vCount);
    if(send_count != vCount)
        ret = MI_ERR_INTERNAL;
    return ret;
}
/****************************************************************************
 * Name: mible_mcu_send_byte
 *
 * Description:
 *   Function for send one byte to uart fd.
 *
 * Input Parameters:
 *   byte  - send byte data.
 * Returned Value:
 *   void.
 *
 ****************************************************************************/
void mible_mcu_send_byte(uint8_t byte)
{
    mible_mcu_uart_send_data(&byte,1);
}
/****************************************************************************
 * Name: mible_mcu_flushinput
 *
 * Description:
 *   Function for flushing mcu uart IO buffer
 *
 * Input Parameters:
 *   void - void
 *
 * Returned Value:
 *   Upon successful completion, 0 is returned. Otherwise, -1 is returned and
 *   errno is set to indicate the error.
 *
 ****************************************************************************/
int mible_mcu_flushinput(void)
{
    return ioctl(fd_mcu_uart, TCFLSH, TCIOFLUSH);
}
/****************************************************************************
 * Name: mible_mcu_recv_bytes
 *
 * Description:
 *   Function for request some bytes from uart before timeout
 *
 * Input Parameters:
 *   bytes  - recv buffer pt.
 *   req_count - should recv count.
 *   timeout - wait recv time.
 * Returned Value:
 *   Upon successful completion, 0 is returned. Otherwise, 1 is returned and
 *   errno is set to indicate the error.
 *
 ****************************************************************************/
mible_status_t mible_mcu_recv_bytes(void *bytes, uint16_t req_count, uint32_t timeout)
{
    rsize_t rev_count;
    mible_status_t ret = MI_SUCCESS;
    
    rev_count = mible_mcu_uart_recv_data(bytes,req_count,timeout);
    if (rev_count !=req_count){
        printk("rev_count:%d !=req_count:%d\n",rev_count,req_count);
        ret = MI_ERR_INTERNAL;
    }
    return ret;
}
/****************************************************************************
 * Name: mible_mcu_recv_bytes
 *
 * Description:
 *   Function for read bytes from uart fd before timeout.
 *
 * Input Parameters:
 *   buf  - recv buffer pt.
 *   count - should recv count.
 *   timeout - wait recv time.
 * Returned Value:
 *   Upon successful read count.
 *
 ****************************************************************************/
int mible_mcu_uart_recv_data(uint8_t *buf, size_t count,uint32_t timeout)
{
    ssize_t ret, nread = 0;
    uint64_t expire_time = k_uptime_get()+timeout;

    while (count != nread) {
        ret = read(fd_mcu_uart, buf + nread, count - nread);
        if(k_uptime_get() > expire_time){
            printk("read timeout:%d\n",nread);
            return nread;
        }
        if (ret < 0) {
            if(errno != EAGAIN)
            {
                printk("read err:%d\n",ret);
                return nread;
            }
            continue;
        }
        
        nread += ret;
    }
    
    return nread;
}
/****************************************************************************
 * Name: mible_mcu_uart_send_data
 *
 * Description:
 *   Function for write bytes to uart fd.
 *
 * Input Parameters:
 *   buf  - send buffer pt.
 *   count - should send count.
 * Returned Value:
 *   Upon successful send count.
 *
 ****************************************************************************/
int mible_mcu_uart_send_data(uint8_t *buf, size_t count)
{
    ssize_t ret, nwritten = 0;

    while (nwritten != count) {
        ret = write(fd_mcu_uart, buf + nwritten, count - nwritten);
        if (ret < 0) {
            return ret;
        }

        nwritten += ret;
    }

    return nwritten;
}
#endif
