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

> *NOTE*: Keep the ``TARGET`` matching between the host and the executed xclbin. 
> If needed, rebuild the host with the appropriate matching ``TARGET`` before running it.

#### 2. Hardware/Software Emulation

``make build_xilinx_spmv_xclbin TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1) CFGID=<link-config-file-Id>(default=1)``

The ``ID`` defines the suffix for intermediate and final outputs in the ``HiHiSpmv/bin/`` directory, and the ``CFGID`` defines the identifier of the linker configuration file (in ``HiHiSpMV/src/kernels/`` directory) used in Vitis.

> *NOTE*: Various paramters could be adjusted in the Kernel-Config (``HiHiSpMV/src/kernels/csr_spmv_repl.cfg``) Link-Config (``HiHiSpMV/src/kernels/single.1.cfg``), XRT.ini (``HiHiSpMV/xrt.ini``) and Definitions (``HiHiSpmv/src/kernels/xlx_definitions.hpp``) files.
Their adjustable settings are listed [below](#adjustable-parameters).


#### 3. Bitstream/Hardware 

> *NOTE*: For avoiding synthesizing the Bitsteam/Hardware, already synthesized Bitstream can be downloaded from the [Zenedo upload](https://doi.org/10.5281/zenodo.12700052).

``make build_xilinx_spmv_xclbin TARGET=hw ID=<output-dir-Id>(default=1) CFGID=<link-config-file-Id>(default=1)``

The ``ID`` and ``CFGID`` are as defined above.

> *NOTE*: Various paramters could be adjusted in the Kernel-Config (``HiHiSpMV/src/kernels/csr_spmv_repl.cfg``) Link-Config (``HiHiSpMV/src/kernels/single.1.cfg``), XRT.ini (``HiHiSpMV/xrt.ini``) and Definitions (``HiHiSpmv/src/kernels/xlx_definitions.hpp``) files.
Their adjustable settings are listed [below](#adjustable-parameters).
 
### Run

#### 1. Download test matrix/matrices

``scripts/collect_data.sh``

> *NOTE*: Matrices could be added in [MartixMarket](https://math.nist.gov/MatrixMarket/formats.html) format.

#### 2. Emulation

``make test_xilinx_spmv TARGET=<hw_emu/sw_emu> ID=<output-dir-Id>(default=1)``

> *NOTE*: Make sure to match the ``TARGET`` between the host and xclbin. Further, various execution paramters could be adjusted in the main [Makefile](#adjustable-parameters) (``HiHiSpmv/Makefile``).

#### 3. Bitstream/Hardware

``make test_xilinx_spmv TARGET=hw ID=<output-dir-Id>(default=1)``

> *NOTE*: Make sure to match the ``TARGET`` between the host and xclbin and on the first time configuring the FPGA, a few seconds of delay is expected. Further, various execution paramters could be adjusted in the main [Makefile](#adjustable-parameters) (``HiHiSpmv/Makefile``).

> *NOTE*: The [Zenedo upload](https://doi.org/10.5281/zenodo.12700052) could be downloaded and the Bitstream from it could be used in order to avoid synthesizing it by the following:

```
wget https://zenodo.org/records/12700052/files/HiHiSpMV-v0.0.1.zip
unzip HiHiSpMV-v0.0.1.zip
cp -r HiHiSpMV-v0.0.1/bin/ bin/
```

## Adjustable Parameters

In the following the adjustable paramters in the main Makefile (``HiHiSpmv/Makefile``), Kernel-Config (``HiHiSpMV/src/kernels/csr_spmv_repl.cfg``) Link-Config (``HiHiSpMV/src/kernels/single.1.cfg``), XRT.ini (``HiHiSpMV/xrt.ini``) and Definitions (``HiHiSpmv/src/kernels/xlx_definitions.hpp``) files, are listed.

### 1. Makefile

> *NOTE*: All of the following Makefile parameters apply to executing the host only.

- ``HW_SIZE``: The maximum side-length of the square tile. Should be matching with the ``VECTOR_SIZE`` in the Definitions file.
- ``XLX_DEVICE_ID``: The device Id. one which the Bitstream will be loaded onto. 
- ``XLX_ITERS``: The number of iterations per launch of the CUs.
- ``XLX_RUNS``: The number of times the CUs are launched.

### 2. Definitions

- ``VECTOR_SIZE``: Defines the maximum side-length of the square tile. Could be adjusted according to the available BRAM blocks.
- ``DEBUG <0-3>``: Applicable in ``sw_emu`` only to log the operations inside each CU.
- Trip-count constants: Used for latency reports generatione e.g ``*_min`` and ``*_max``.

### 3. [Link-Config](https://docs.amd.com/r/2022.2-English/ug1393-vitis-application-acceleration/Getting-Started-with-Vitis)

- HBM assignments: Assigns each CU's HBM bank connections e.g. ``sp=csr_spmv_repl_1_1.indices:HBM[0]``. Should be matching with the number of CUs to be used.
- Stream connections: Defines intra-CU AXI-stream connections e.g. ``sc=csr_spmv_repl_1_4.out_indices:csr_spmv_repl_2_4.in_indices:64``. Should be matching with the number of CUs to be used.
- SLR assignment: Assigns each CU to a specific SLR e.g. ``slr=csr_spmv_repl_1_1:SLR2``.
 Should be matching with the number of CUs to be used.
- Number of kernels: Defines number of CUs for each kernel e.g. ``nk=csr_spmv_repl_1:16``.

### 4. [XRT.ini](https://docs.amd.com/r/2022.2-English/ug1393-vitis-application-acceleration/xrt.ini-File)


Should use legacy scheduler by specifying the following: 
```
[Runtime] 
ert=false
kds=flase
```
In order to run all 16 CUs, i.e. more than 32 kernels.

## To cite

#### Bibtex

    @inproceedings{ta-me-24a,
    Author = {Abdul Rehman Tareen and Marius Meyer and Christian Plessl and Tobias Kenter},
    Title = {{HiHiSpMV}: Sparse Matrix Vector Multiplication with Hierarchical Row Reductions on {FPGAs} with High Bandwidth Memory},
    Booktitle = {Proc. IEEE Symp. on Field-Programmable Custom Computing Machines (FCCM)},
    Year = {2024},
    Note = {To appear.}}

