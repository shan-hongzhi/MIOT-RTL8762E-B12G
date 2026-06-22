/****************************************************************************
 * vendor/realtek/chips/rtl8762e/rtl876x_irq.c
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

#include <inttypes.h>
#include <assert.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include "arm_internal.h"
#include "nvic.h"
#include "irq.h"
#include "system_rtl876x.h"
#include "rtl876x.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define DEFPRIORITY32 \
  (NVIC_SYSH_PRIORITY_DEFAULT << 24 | NVIC_SYSH_PRIORITY_DEFAULT << 16 | \
   NVIC_SYSH_PRIORITY_DEFAULT << 8  | NVIC_SYSH_PRIORITY_DEFAULT)
#define VTOR_RAM_ADDR               0x00200000 //!< vector table address in RAM.

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 ****************************************************************************/

bool rtl_table_update(void)
{
  extern void __RTL_ROM_Default_Handler(void);
  extern uint32_t *__ram_text_start__;

  void **__vectors = (void*)&__ram_text_start__;

  IRQ_Fun *vectors = (IRQ_Fun *)VTOR_RAM_ADDR;

  for (uint32_t i = 16; i < NR_IRQS; ++i)
    {
      if ((IRQ_Fun)__RTL_ROM_Default_Handler != vectors[i])
        {
          irq_attach(i, (xcpt_t)vectors[i], NULL);
        }
        NVIC_SetPriority(i - 16, 0);
    }

  __vectors[2] = (xcpt_t)vectors[2];

  return true;
}

extern uint8_t _stextram[];
extern uint8_t _etextram[];

#if !defined(EXTRA_FMT) && !defined(EXTRA_ARG) && defined(CONFIG_HAVE_FUNCTIONNAME)
#  define EXTRA_FMT "%s: "
#  define EXTRA_ARG ,__FUNCTION__
#endif

#ifndef EXTRA_FMT
#  define EXTRA_FMT
#endif

#ifndef EXTRA_ARG
#  define EXTRA_ARG
#endif

#ifdef CONFIG_DEBUG_HARDFAULT_ALERT
# define hfalert(format, ...)  syslog(LOG_EMERG, EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#else
# define hfalert(x...)
#endif

#ifdef CONFIG_DEBUG_HARDFAULT_INFO
# define hfinfo(format, ...)   _info(format, ##__VA_ARGS__)
#else
# define hfinfo(x...)
#endif

#define INSN_SVC0        0xdf00 /* insn: svc 0 */

static int __arm_hardfault(int irq, void *context, void *arg)
{
  uint32_t *regs = (uint32_t *)context;

  /* Get the value of the program counter where the fault occurred */

  uint16_t *pc = (uint16_t *)regs[REG_PC] - 1;

  /* Check if the pc lies in known FLASH memory.
   * REVISIT:  What if the PC lies in "unknown" external memory?
   */

#ifdef CONFIG_BUILD_PROTECTED
  /* In the kernel build, SVCalls are expected in either the base, kernel
   * FLASH region or in the user FLASH region.
   */

  if (((uintptr_t)pc >= (uintptr_t)_stext &&
       (uintptr_t)pc <  (uintptr_t)_etext) ||
      ((uintptr_t)pc >= (uintptr_t)USERSPACE->us_textstart &&
       (uintptr_t)pc <  (uintptr_t)USERSPACE->us_textend))
#else
  /* SVCalls are expected only from the base, kernel FLASH region */

  if (((uintptr_t)pc >= (uintptr_t)_stextram &&
       (uintptr_t)pc < (uintptr_t)_etextram) ||
      ((uintptr_t)pc >= (uintptr_t)_stext &&
       (uintptr_t)pc < (uintptr_t)_etext))
#endif
    {
      /* Fetch the instruction that caused the Hard fault */

      uint16_t insn = *pc;
      hfinfo("  PC: %p INSN: %04x\n", pc, insn);

      /* If this was the instruction 'svc 0', then forward processing
       * to the SVCall handler
       */

      if (insn == INSN_SVC0)
        {
          hfinfo("Forward SVCall\n");
          return arm_svcall(irq, context, NULL);
        }
    }

#if defined(CONFIG_DEBUG_HARDFAULT_ALERT)
  /* Dump some hard fault info */

  hfalert("\nHard Fault:\n");
  hfalert("  IRQ: %d regs: %p\n", irq, regs);
  hfalert("  PRIMASK: %08x IPSR: %08lx\n",
          getprimask(), getipsr());
#endif

  up_irq_save();

  hfalert("PANIC!!! Hard fault\n");
  PANIC_WITH_REGS("panic", context);
  return OK; /* Won't get here */
}

void up_irqinitialize(void)
{
  uint32_t regaddr;
  int i;

  /* Disable all interrupts */

  putreg32(0xffffffff, ARMV6M_NVIC_ICER);

  /* Set all interrupts (and exceptions) to the default priority */

  putreg32(DEFPRIORITY32, ARMV6M_SYSCON_SHPR2);
  putreg32(DEFPRIORITY32, ARMV6M_SYSCON_SHPR3);

  /* Now set all of the interrupt lines to the default priority */

  for (i = 0; i < 8; i++)
    {
      regaddr = ARMV6M_NVIC_IPR(i);
      putreg32(DEFPRIORITY32, regaddr);
    }

  /* Attach the SVCall and Hard Fault exception handlers.  The SVCall
   * exception is used for performing context switches; The Hard Fault
   * must also be caught because a SVCall may show up as a Hard Fault
   * under certain conditions.
   */

  irq_attach(SVC_VECTORn, arm_svcall, NULL);
  irq_attach(HardFault_VECTORn, __arm_hardfault, NULL);
  putreg32((uint32_t)_vectors, ARMV6M_SYSCON_VECTAB);

  /* Initialize logic to support a second level of interrupt decoding for
   * configured pin interrupts.
   */

#ifdef CONFIG_GPIOIRQ
  gpioirqinitialize();
#endif

#ifndef CONFIG_SUPPRESS_INTERRUPTS

  /* And finally, enable interrupts */

  up_irq_enable();
#endif
}

void arm_ack_irq(int irq)
{
}

/****************************************************************************
 * Name: up_enable_irq
 ****************************************************************************/

void up_enable_irq(int irq)
{
  DEBUGASSERT((unsigned)irq < NR_IRQS);

  /* Check for external interrupt */

  irq -= 16;
  if (irq >= Peripheral_First_IRQn)
    {
      PERIPHINT->EN |= BIT(irq - Peripheral_First_IRQn);
      irq = Peripheral_IRQn;
    }

    NVIC_ClearPendingIRQ(irq);
    NVIC_SetPriority(irq, 0);
    NVIC_EnableIRQ(irq);
}

/****************************************************************************
 * Name: up_disable_irq
 ****************************************************************************/

void up_disable_irq(int irq)
{
  DEBUGASSERT((unsigned)irq < NR_IRQS);

  /* Check for an external interrupt */

  irq -= 16;
  if (irq >= Peripheral_First_IRQn)
    {
      PERIPHINT->EN &= ~BIT(irq - Peripheral_First_IRQn);

      if (PERIPHINT->EN == 0)
        {
          /* Disable Peripheral IRQ Channel */

          NVIC_DisableIRQ(Peripheral_IRQn);
        }
    }
  else
    {
      /* Disable the Selected IRQ Channels */

      NVIC_DisableIRQ(irq);
    }
}
