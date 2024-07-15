# Host architecture specification(Check if use anywhere, else remove)
HOST_ARCH := x86 

# Profile the application
PROFILE := no

# Debug instrument the application
DEBUG := no

# Compilation target options: sw_emu, hw_emu, hw ...
TARGET := sw_emu

# GCC CPP flags for xilinx host
CXXFLAGS_XILINX += -DTARGET=$(TARGET)

# Optimization flag for the synthesis
O := 0

# Temp. Id for a synthesis
ID := 1

# Temp. Id for linking config file
CFGID := 1

# XRT and VIVADO includes and libs
XRT_INCLUDE:= $(XILINX_XRT)/include
XRT_LIBS:= $(XILINX_XRT)/lib/
VIVADO_INCLUDE:= $(XILINX_VIVADO)/include

# Xilinx host compilation flags
CXXFLAGS_XILINX += -I$(XRT_INCLUDE) -I$(VIVADO_INCLUDE) -O3 -std=c++17 -lpthread -lrt -lstdc++ -fmessage-length=0 #-Wall

EXEC_PRE_COMMAND := EMCONFIG_PATH=. XRT_INI_PATH=xrt.ini

# TARGET is emu the add debug flag and also set pre command
ifeq ($(TARGET), $(filter $(TARGET),sw_emu hw_emu))
CXXFLAGS_XILINX += -g 
# Pre-execute variable used for emulation
EXEC_PRE_COMMAND += XCL_EMULATION_MODE=$(TARGET)
endif

# Append src and include directories for sources
CXXFLAGS_XILINX += -I'$(SRC_ROOT)'' -I'$(INC_ROOT)''

# Xilinx host linker flags
CXXLDFLAGS_XILINX :=-L$(XRT_LIBS) -pthread -lOpenCL -lrt -lstdc++ -luuid -lxrt_coreutil

#Generates profile summary report
ifeq ($(PROFILE), yes)
VPP_LDFLAGS += --profile.data all:all:all
endif

#Generates debug summary report
ifeq ($(DEBUG), yes)
# Vitis linker flags
VPP_LDFLAGS += --dk list_ports
endif

VPP_LDFLAGS += -l

# Xilinx host source and binary
XLX_SPMV_HOST_SRC := $(XLX_ROOT)/xlx_spmv_host.cpp  

XLX_SPMV_HOST_BIN := $(BIN_DIR)/xilinx_spmv_host

# VPP temp dir, to keep the obj and report files
XLX_TEMP_DIR := $(BIN_DIR)/temp_dir.$(TARGET).$(ID)

# VPP build dir, to keep the xclbin and other files
XLX_BUILD_DIR := $(BIN_DIR)/build_dir.$(TARGET).$(ID)

VPP_FLAGS := --platform $(PLATFORM) --target $(TARGET) --optimize $(O)
VPP_FLAGS += --temp_dir $(XLX_TEMP_DIR) -I'$(SRC_ROOT)' -I'$(INC_ROOT)' -I'$(SRC_REF)' --save-temps

XLX_KRN_DIR := $(XLX_ROOT)/kernels

############# Kernel names
####### SpMV

#### CSR

## Model 2
XLX_SPMV_CSR_MODEL_2_REP := csr_spmv_repl

# !!!! Comment out the kernels you don't want to incldue in the xclbin !!!!

###### Model 2: 4 kernel replicated x
XLX_SINGLE_OBJS += $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_1.xo
XLX_SINGLE_OBJS += $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_2.xo
XLX_SINGLE_OBJS += $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_1.xo
XLX_SINGLE_OBJS += $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_2.xo

# One xclbin to contain all single versions
XLX_SINGLE_XCLBIN := $(XLX_BUILD_DIR)/hihi_spmv.xclbin

######################### Single Precision compilation targets #####################

###### Model 2: 4 kernel replicated x
$(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_1.xo: $(XLX_KRN_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_1.cpp .pre_xilinx
	$(VPP) $(VPP_FLAGS) --config '$(<D)/$(XLX_SPMV_CSR_MODEL_2_REP).cfg' -c -k csr_spmv_repl_1 -I'$(XLX_KRN_DIR)' -o'$@' '$<'
$(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_2.xo: $(XLX_KRN_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_2.cpp .pre_xilinx
	$(VPP) $(VPP_FLAGS) --config '$(<D)/$(XLX_SPMV_CSR_MODEL_2_REP).cfg' -c -k csr_spmv_repl_2 -I'$(XLX_KRN_DIR)' -o'$@' '$<'
$(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_1.xo: $(XLX_KRN_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_3_1.cpp .pre_xilinx
	$(VPP) $(VPP_FLAGS) --config '$(<D)/$(XLX_SPMV_CSR_MODEL_2_REP).cfg' -c -k csr_spmv_repl_3 -I'$(XLX_KRN_DIR)' -o'$@' '$<'
$(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_2.xo: $(XLX_KRN_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_3_2.cpp .pre_xilinx
	$(VPP) $(VPP_FLAGS) --config '$(<D)/$(XLX_SPMV_CSR_MODEL_2_REP).cfg' -c -k csr_spmv_repl_4 -I'$(XLX_KRN_DIR)' -o'$@' '$<'

# XCLBIN compilation targets 
$(XLX_SINGLE_XCLBIN): $(XLX_SINGLE_OBJS)
	$(VPP) $(VPP_FLAGS) $(VPP_LDFLAGS) --config '$(XLX_KRN_DIR)/single.$(CFGID).cfg' -o'$@' $(+) 

# k1: $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_1.xo
# k2: $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_2.xo
# k3: $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_1.xo
# k4: $(XLX_TEMP_DIR)/$(XLX_SPMV_CSR_MODEL_2_REP)_s_3_2.xo

check_xilinx:
	ifndef XILINX_VIVADO
		$(error XILINX_VIVADO variable is not set.)
	endif

	ifndef XILINX_XRT
		$(error XILINX_XRT variable is not set.)
	endif

	ifndef PLATFORM
		$(error PLATFORM not set.)
	endif