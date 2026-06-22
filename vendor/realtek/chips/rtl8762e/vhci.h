#ifndef _VHCI_H_
#define _VHCI_H_

#include <hci_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup VHCI VHCI interface
 *
 * VHCI interface between upper stack and lower stack.
 * @{
 */
/**
 * @brief Open lower stack VHCI interface.
 *
 * This function also resets lower stack VHCI.
 * @param p_callback
 * @return true if successful; false on error.
 */
bool __rtl_vhci_open(P_HCI_IF_CALLBACK p_callback);

/**
 * @brief Close lower stack VHCI interface.
 *
 * This function also resets lower stack VHCI.
 * @return true if successful; false on error.
 */
bool vhci_close(void);

/**
 * @brief Send packet from upper stack to lower stack.
 *
 * Upper stack should use this function to send packet to lower stack.\n
 * \p p_buf should start with packet type.
 * @param p_buf  Packet buffer.
 * @param len    Packet length.
 * @return true if successful; false on error.
 */
extern bool (*__rtl_vhci_send)(void *p_buf, uint32_t len);

/**
 * @brief Send packet acknowledgment response from upper stack to lower stack.
 *
 * Upper stack should use this function to ACK packet successfully received
 * from lower stack.
 * @param p_buf  Packet buffer.
 * @return true if successful; false on error.
 */
extern bool (*__rtl_vhci_ack)(void *p_buf);

/**
 * @brief Send packet acknowledgment response from upper stack to lower stack
 * without notify lower stack task.
 *
 * Upper stack should use this function to ACK packet successfully received
 * from lower stack.
 * @param p_buf  Packet buffer.
 * @return true if successful; false on error.
 */
extern bool (*vhci_ack_nowake)(void *p_buf);

/**
 * @brief Check if there are VHCI related signals pended.
 *
 * If nowake versions of VHCI functions are used, you can use this function to
 * check the existence of pended VHCI signals. If there are pended VHCI signals,
 * you may want to wake up lower stack task to handle those signals for
 * real-time issues. *
 * @return  \c true if there are pended VHCI signals; otherwise, \c false.
 */
bool vhci_has_sig_pended(void);
/**@} */

#ifdef __cplusplus
}
#endif

#endif /* _VHCI_H_ */
