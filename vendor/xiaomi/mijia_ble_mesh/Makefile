
-include $(APPDIR)/Make.defs

CFLAGS += -DuECC_VLI_NATIVE_LITTLE_ENDIAN=0

ifeq ($(CONFIG_MIBLE_SDK_LOG),y)
CFLAGS += -DMI_LOG_ENABLED=1 -DMI_ASSERT=1
endif

CFLAGS += -Wno-pointer-sign -Wno-strict-prototypes -Wno-shadow -Wno-undef -Wno-unused-function -Wno-unused-variable -Wno-unused-label -Wno-unused-but-set-variable

BOARD ?= $(patsubst "%",%,$(CONFIG_MIBLE_BOARD_NAME))

CSRCS += src/board/$(BOARD)/mi_mesh_otp.c
CSRCS += src/board/$(BOARD)/nvm.c
CSRCS += src/mi_crypto_backend_mbedtls.c
CSRCS += src/mible_crypto.c
CSRCS += src/mible_user_api.c
CSRCS += src/mible_mcu.c
CSRCS += src/xmodem.c
CSRCS += src/third_party/mbedtls/ccm.c
CSRCS += src/third_party/mbedtls/sha256_hkdf.c
CSRCS += src/third_party/mbedtls/md.c
CSRCS += src/third_party/mbedtls/md_wrap.c
CSRCS += src/third_party/mbedtls/sha256.c
CSRCS += src/third_party/mbedtls/asn1parse.c
CSRCS += src/third_party/mbedtls/base64.c
CSRCS += src/third_party/micro-ecc/uECC.c
ifneq ($(CONFIG_MIBLE_PSK),y)
CSRCS += src/model.c
ifneq ($(CONFIG_PM),y)
CSRCS += src/board/$(BOARD)/light.c
endif
endif
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/zblue/port/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/zblue/port/include/kernel/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/zblue/zblue/include/zephyr}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/tinycrypt/lib/include}
CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/vendor/realtek/chips/rtl8762e}

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" src}

ifeq ($(CONFIG_MIBLE_PSK),y)
	PROGNAME += mible
	MAINSRC  += src/board/$(BOARD)/main_ble.c
else
	PROGNAME += mimesh
	ifeq ($(CONFIG_PM),y)
		ifeq ($(CONFIG_BT_PER_ADV_SYNC),y)
			MAINSRC  += src/board/$(BOARD)/main_lpn_pa.c
		else
			MAINSRC  += src/board/$(BOARD)/main_lpn.c
		endif
	else
		MAINSRC  += src/board/$(BOARD)/main.c
	endif
endif

PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = CONFIG_MIBLE_SDK_STACK_SIZE
MODULE    = y

include $(APPDIR)/Application.mk
