# HiHiSpMV

HiHiSpMV is a Sparse Matrix with dense Vector multiplication (SpMV) accelerator for HBM equipped FPGAs. Its details could be found [here](tobeadded.com).

## Dependencies
* VITIS 22.2/XRT 2.14 and AMD/Xilinx xilinx_u280_gen3x16_xdma_1_202211_1 shell for a AMD/Xilinx U280 FPGA card.
* GCC & C++17 standard
* GNU Make
* Bash (for downloading and processing test matrices)
* Python3 (for downloading and processing test matrices)

## How to

### Load modules on Noctua 2

```
ml reset
ml fpga
ml xilinx/xrt/2.14
```

### Build

For the followings, run the commands in the main *HiHiSpMV* directory.

#### 1. Host

``make build_xilinx_spmv_host TARGET=<hw/hw_emu/sw_emu> O=<gcc-Olevel>``

#### 2. Hardware/Software Emulation

``make build_xilinx_spmv_xclbin TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1) CFGID=<link-config-file-Id>(default=1)``

> *NOTE*: Various paramters could be adjusted in link-config, xrt.ini and ``HiHiSpmv/src/kernels/xlx_definitions.hpp`` files.


#### 3. Bitstream/Hardware 

``make build_xilinx_spmv_xclbin TARGET=hw ID=<output-dir-Id>(default=1) CFGID=<link-config-file-Id>(default=1)``

> *NOTE*: Various paramters could be adjusted in link-config, xrt.ini and ``HiHiSpmv/src/kernels/xlx_definitions.hpp`` files.

### Run

#### 1. Download test matrix/matrices

``scripts/collect_data.sh``

> *NOTE*: Matrices could be added in [MartixMarket](https://math.nist.gov/MatrixMarket/formats.html) format.

#### 2. Emulation

``make test_xilinx_spmv TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1)``

> *NOTE*: Various paramters could be adjusted in the main Makefile.


#### 3. Bitstream/Hardware

``make test_xilinx_spmv TARGET=hw ID=<output-dir-Id>(default=1)``

> *NOTE*: Various paramters could be adjusted in the main Makefile.


## To cite

#### Bibtex

    @inproceedings{ta-me-24a,
    Author = {Abdul Rehman Tareen and Marius Meyer and Christian Plessl and Tobias Kenter},
    Title = {{HiHiSpMV}: Sparse Matrix Vector Multiplication with Hierarchical Row Reductions on {FPGAs} with High Bandwidth Memory},
    Booktitle = {Proc. IEEE Symp. on Field-Programmable Custom Computing Machines (FCCM)},
    Year = {2024},
    Note = {To appear.}}

