# HiHiSpMV

HiHiSpMV is a Sparse Matrix with dense Vector multiplication (SpMV) accelerator for HBM equipped FPGAs. Its details could be found in the FCCM [publication](tobeadded.com).

## Dependencies
* VITIS/XRT 2022.2 and AMD/Xilinx xilinx_u280_gen3x16_xdma_1_202211_1 shell for a AMD/Xilinx U280 FPGA card.
* GCC & C++17 standard
* GNU Make
* Bash (for downloading and processing test matrices)
* Python3 (for downloading and processing test matrices)

## How to

### Build

For the followings, run the commands in the main *HiHiSpMV* directory.

#### 1. Host

``make build_xilinx_spmv_host TARGET=<hw/hw_emu/sw_emu>``

#### 2. Hardware/Software Emulation

``make build_xilinx_spmv_host TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1) CFGI=<link-config-file-Id>(default=1)``

> [!NOTE] Various paramters could be adjusted in link-config, xrt.ini and "HiHiSpmv/src/kernels/xlx_definitions.hpp" files.


#### 3. Bitstream/Hardware 

``make build_xilinx_spmv_host TARGET=<hw> ID=<output-dir-Id>(default=1) CFGID=<link-config-file-Id>(default=1)``

> [!NOTE]  Various paramters could be adjusted in link-config, xrt.ini and ``HiHiSpmv/src/kernels/xlx_definitions.hpp`` files.

### Run

#### 1. Download test matrix/matrices

``scripts/collect_data.sh``

> [!NOTE] Matrices could be added in [MartixMarket](https://math.nist.gov/MatrixMarket/formats.html) format.

#### 2. Emulation

``make test_xilinx_spmv TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1)``

> [!NOTE] Various paramters could be adjusted in the main Makefile.


#### 3. Bitstream/Hardware

``make test_xilinx_spmv TARGET=<hw> ID=<output-dir-Id>(default=1)``

> [!NOTE] Various paramters could be adjusted in the main Makefile.


## To cite

#### Bibtex

    @article{hihispmv,
        author = {Abdul Rehman Tareen and Marius Meyer and Tobias Kenter and Christian Plessl},
        doi = {},
        issn = {},
        keywords = {SpMV, FPGA, XRT, Vitis HLS, High level synthesis, HPC},
        pages = {},
        title = {HiHiSpMV: Sparse Matrix Vector Multiplication with Hierarchical Row Reductions on FPGAs with High Bandwidth Memory},
        url = {},
        volume = {},
        year = {2024}
    }

