
CC = $(CROSS_COMPILE)gcc --sysroot=$(SDKTARGETSYSROOT)
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip 

#CFLAGS ?= -O2 -Wall

DEST_DIR = ./bin
RELEASE_DIR = ./bin
HANTRO_G1G2_LIB_DIR = ../../imx-vpu-hantro
HANTRO_VC8000E_LIB_DIR = ../../imx-vpu-hantro-vc
HANTRO_H1_LIB_DIR = ../../imx-vpu-hantro
CTRLSW_HDRPATH = ../inc

DAEMON_DIR = ./v4l2_vsi_daemon
#KERNEL_DIR = ../../linux-2.6-testbuild
#MXC_UNIT_TEST_DIR = ./v4l2_vsi_daemon/test/mxc
#MXC_UNIT_TEST = mxc_v4l2_vpu_enc.out

OUT_FILE = vsidaemon

DEC = nxp_enc_hw
ENC = nxp_dec_hw
LIBFILES = 
LD_FLAGS = $(LDFLAGS) -lpthread -lm

PLATFORM ?= IMX8MP
TSM_ENA ?= 0  # enable/disable timestamp manager

ifeq ($(PLATFORM), IMX8MP)
TARGET=865
LD_FLAGS += -L$(HANTRO_G1G2_LIB_DIR) -lhantro -lg1 -L$(HANTRO_VC8000E_LIB_DIR) -lhantro_vc8000e
else ifeq ($(PLATFORM), IMX8MM)
TARGET=845
LD_FLAGS += -L$(HANTRO_G1G2_LIB_DIR) -lhantro -lg1 -L$(HANTRO_H1_LIB_DIR) -lhantro_h1
else ifeq ($(PLATFORM), IMX8MQ)
TARGET=850
LD_FLAGS += -L$(HANTRO_G1G2_LIB_DIR) -lhantro -lg1
else
TARGET=865
LD_FLAGS += -L$(HANTRO_G1G2_LIB_DIR) -lhantro -lg1 -L$(HANTRO_VC8000E_LIB_DIR) -lhantro_vc8000e
endif


all:
	@mkdir -p $(RELEASE_DIR)
	make -C $(DAEMON_DIR) CC="$(CC)" OUT_FILE=$(OUT_FILE) target=$(TARGET) tsm_ena=$(TSM_ENA) enc=$(ENC) dec=$(DEC) LIBFILES="$(LIBFILES)" LD_FLAGS="$(LD_FLAGS)" CTRLSW_HDRPATH="$(CTRLSW_HDRPATH)"
	cp $(DAEMON_DIR)/$(OUT_FILE) $(RELEASE_DIR)/$(OUT_FILE)

install:
	@mkdir -p $(DEST_DIR)/usr/bin
	cp $(RELEASE_DIR)/$(OUT_FILE) $(DEST_DIR)/usr/bin/

#test:
#	@mkdir -p $(RELEASE_DIR)
#	make -C $(MXC_UNIT_TEST_DIR) CC="$(CC)" STRIP=$(STRIP) debug=0 VSIPARSE=y
#	cp $(MXC_UNIT_TEST_DIR)/$(MXC_UNIT_TEST) $(RELEASE_DIR)/$(MXC_UNIT_TEST)

.PHONY: clean
clean:
	make -C $(DAEMON_DIR) CC="$(CC)" OUT_FILE=$(OUT_FILE) target=$(TARGET) enc=$(ENC) dec=$(DEC) LIBFILES="$(LIBFILES)" LD_FLAGS="$(LD_FLAGS)" CTRLSW_HDRPATH="$(CTRLSW_HDRPATH)" clean
	#make -C $(MXC_UNIT_TEST_DIR) CC="$(CC)" STRIP=$(STRIP) debug=0 VSIPARSE=y clean
	rm -f $(RELEASE_DIR)/$(OUT_FILE)
	#rm -f $(RELEASE_DIR)/$(MXC_UNIT_TEST)

