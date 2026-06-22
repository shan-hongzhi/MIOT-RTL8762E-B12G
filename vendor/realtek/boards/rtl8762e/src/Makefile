include $(TOPDIR)/Make.defs

CSRCS = system_rtl876x.c

AST_FILE        = $(TOPDIR)/sched/misc/assert.c

AST_LOOP_LABEL  = '__loop:\n'

AST_STACK_DUMP  = '  show_stacks(rtcb, up_getusrsp(regs));'
AST_STACK_DUMP1 = '  show_stacks(rtcb, up_getusrsp(regs)) ;'
AST_RAM_DUMP    = '  stack_dump(0x00200000, 0x00200000 + (88 * 1024));'
AST_RAM_DUMP1   = '  stack_dump(0x00280000, 0x00280000 + (16 * 1024));'

AST_LOOP1       = '  if (!regs)'
AST_LOOP        = '  if (regs == NULL)'

AST_GOTO_LABEL  = '          goto __loop;'
AST_GOTO1       = '          up_mdelay(99999);'
AST_GOTO        = '          up_mdelay(250);'

depend:
	$(Q) sed 's/'$(AST_LOOP)'/'$(AST_LOOP_LABEL)'\n'$(AST_LOOP1)'/g' -i $(AST_FILE)
	$(Q) sed 's/'$(AST_STACK_DUMP)'/'$(AST_STACK_DUMP1)'\n'$(AST_RAM_DUMP)'\n'$(AST_RAM_DUMP1)'/g' -i $(AST_FILE)
	$(Q) sed 's/'$(AST_GOTO)'/'$(AST_GOTO1)'\n'$(AST_GOTO_LABEL)'/g' -i $(AST_FILE)

include $(TOPDIR)/boards/Board.mk
