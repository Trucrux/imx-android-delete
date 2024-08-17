#-------------------------------------------------------------------------
# Makefile
#
# Copyright (c) VeriSilicon Beijing
# All rights reserved.
#-------------------------------------------------------------------------
 
#Building Target
#target: 845, 850, ,865

ifeq ($(strip $(target)),)
target=865
endif

CC = $(CROSS_COMPILE)gcc --sysroot=$(SDKTARGETSYSROOT)
C++ = $(CROSS_COMPILE)g++ --sysroot=$(SDKTARGETSYSROOT)
AR = $(CROSS_COMPILE)ar
RM=rm -rf

CFLAGS = -fPIC -Wall -Wno-pointer-sign -Wno-unused-function -DUSE_EXTERNAL_BUFFER -DUSE_OUTPUT_RELEASE
LD_FLAGS = -lpthread -lm
ifeq ($(debug),1)
CFLAGS += -g -DDEBUG -O0
else ifeq ($(debug),2)
CFLAGS += -g -DDEBUG -O0 -DDEC_COPY_INPUT 
else
CFLAGS += -O2
endif

# OBJECT FILES
OUT_FILE = vsidaemon
SRC_OBJECTS = dec_dpb_buff.o buffer_list.o command_defines.o fifo.o hash_table.o object_heap.o	\
              vsi_dec.o vsi_main.o

ifeq ($(target),865)
SRC_OBJECTS += vsi_dec_hevc.o vsi_dec_vp9.o vsi_dec_h264.o vsi_dec_vp8.o vsi_enc_img_h2.o vsi_enc.o vsi_enc_video_h2.o
CFLAGS += -I$(CTRLSW_HDRPATH)/hantro_dec -I$(CTRLSW_HDRPATH)/hantro_VC8000E_enc -DUSE_G2 -DUSE_G1 -DUSE_HW -DNXP -DHAS_VSI_ENC
LD_FLAGS += -L. -lhantro -lg1 -lhantro_vc8000e

else ifeq ($(target),845)
SRC_OBJECTS += vsi_dec_hevc.o vsi_dec_vp9.o  vsi_dec_h264.o vsi_dec_vp8.o vsi_enc_img_h1.o vsi_enc.o vsi_enc_video_h1.o
CFLAGS += -I$(CTRLSW_HDRPATH)/hantro_dec -I$(CTRLSW_HDRPATH)/hantro_enc -DUSE_G2 -DUSE_G1 -DUSE_HW -DUSE_H1 -DNXP -DHAS_VSI_ENC
LD_FLAGS += -L. -lhantro -lg1 -lhantro_h1

else ifeq ($(target),850)
SRC_OBJECTS += vsi_dec_hevc.o vsi_dec_vp9.o vsi_dec_h264.o vsi_dec_mpeg2.o vsi_dec_vp8.o vsi_dec_mpeg4.o vsi_dec_vc1.o vsi_dec_jpeg.o vsi_dec_rv.o vsi_dec_avs.o
CFLAGS += -I$(CTRLSW_HDRPATH)/hantro_dec -DUSE_G2 -DUSE_G1 -DUSE_HW -DHAS_FULL_DECFMT
LD_FLAGS += -L. -lhantro -lg1

else
#echo "invalid building target"
endif

ifneq ($(tsm_ena),0)
CFLAGS += -DNXP_TIMESTAMP_MANAGER
SRC_OBJECTS += dec_ts.o
endif

SRC_DIR = src
OBJ_DIR = obj
	
OBJECTS = $(addprefix $(OBJ_DIR)/, $(SRC_OBJECTS))

.PHONY : all

all : $(OUT_FILE)
	
clean:
	$(RM) $(OBJ_DIR)/*.o 
	$(RM) $(OUT_FILE)

$(OUT_FILE) : $(OBJECTS)
	$(CC) -o $(OUT_FILE) $(OBJECTS) $(LD_FLAGS)
	
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $@ -c $(patsubst %.o,%.c , $(patsubst $(OBJ_DIR)/%,$(SRC_DIR)/%,$@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(C++) $(CFLAGS) -o $@ -c $(patsubst %.o,%.cpp , $(patsubst $(OBJ_DIR)/%,$(SRC_DIR)/%,$@))
