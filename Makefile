# Todo: write help for this Makefile which is printed in a target

# --------------------------------    General definitions     --------------------------------

# CPP compiler
CC = g++

# Python interpreter
PYTHON = python3

# Vitis V++ compiler
VPP = v++

RM = rm -rf

# Makefile path (absolute)
MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))

# Project path
PROJ_ROOT := $(patsubst %/,%,$(dir $(MK_PATH)))
SRC_ROOT :=  $(PROJ_ROOT)/src
INC_ROOT :=  $(PROJ_ROOT)/include

BIN_DIR := $(PROJ_ROOT)/bin

# Xilinx root directory
XLX_ROOT := $(SRC_ROOT)

# Refrence source directory
SRC_REF := $(SRC_ROOT)/reference

# Test matrix data path 
DATA_PATH := $(PROJ_ROOT)/data

# ----------------------------------  Xilinx definitions  ------------------------------------

# Implicit targets for kernels and related utilities are defined in the following included file
include ./xilinx.mk

XLX_EXEC_ARGS += $(XLX_SINGLE_XCLBIN) #bin/build_dir.hw.1/hihispmv.xclbin 

## Some interesting matrices
XLX_MATRIX		:= psmigr_2/psmigr_2_row_sorted.mtx
XLX_DEVICE_ID	:= 1	# Deviced Id
XLX_TEST		:= 0	# Test Type
XLX_CU_COUNT	:= 16	# Compute Units
XLX_TILES		:= 0	# Tiles in a partition - Inactive for now
XLX_PART_METHOD := 2	# Partition method
XLX_ITERS		:= 100	# Iterations
XLX_RUNS		:= 1	# Runs
HW_SIZE			:= 1875	# Hardware size (max. square tile size)

XLX_EXEC_ARGS += $(DATA_PATH)/$(XLX_MATRIX) $(XLX_DEVICE_ID) $(XLX_TEST) $(XLX_CU_COUNT) \
					$(XLX_TILES) $(HW_SIZE) $(XLX_PART_METHOD) $(XLX_ITERS) $(XLX_RUNS)

# ----------------------------------------  Pre targets  -------------------------------------

.pre:
	mkdir -p $(BIN_DIR)

.pre_xilinx: .pre
	mkdir -p $(XLX_TEMP_DIR)
	mkdir -p $(XLX_BUILD_DIR)

# -------------------------------- Xilinx specific targets --------------------------------

build_xilinx_spmv_host: .pre
	$(CC) $(CXXFLAGS_XILINX) $(XLX_SPMV_HOST_SRC) -o $(XLX_SPMV_HOST_BIN) $(CXXLDFLAGS_XILINX) 

# ------------ Explicit XO compilation targets for implcit targets in "xilinx.mk" ------------

# Explicit XCLBIN compilation targets for implcit targets in "xilinx.mk"
build_xilinx_spmv_xclbin: $(XLX_SINGLE_XCLBIN)

# Emulator config. generation
emconfig:
ifeq ($(TARGET),$(filter $(TARGET), sw_emu hw_emu))
	emconfigutil --platform $(PLATFORM) --od bin/
endif

# Execute xilinx 
test_xilinx_spmv:  
	$(EXEC_PRE_COMMAND) $(XLX_SPMV_HOST_BIN) $(XLX_EXEC_ARGS) 

# -------------------------------- Misc. targets  --------------------------------

clean: # Todo: Add the remaining cleanup stuff here
	$(RM) "bin"
	git clean -dXf
