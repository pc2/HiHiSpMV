/*
MIT License

Copyright (c) 2024 Abdul Rehman Tareen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>
#include <iomanip>
#include <string>
#include <math.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <random>    
#include <fenv.h>
#include <bitset>

#include "../include/includes.hpp"
#include "../include/dense_vector.hpp"
#include "../include/csr_matrix.hpp"
#include "../include/csc_matrix.hpp"
#include "../include/linear_algebra.hpp"
#include "partitioning_utility.hpp"
#include "xrt_utility.hpp"
#include "utility.hpp"

// XRT includes
#include <xrt/xrt_device.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_kernel.h>
#include <experimental/xrt_xclbin.h>
#include <experimental/xrt_queue.h>
#include "xclbin.h"
#include "experimental/xrt_profile.h"

#define BLOCK_SIZE 16

// ------ Four Kernel Group CSR SpMV "Multi-tile" on FPGA  ------

template<typename T>
int RunHiHiSpMV(
        std::string binaryFile, 
        std::string matrixFile, 
        int deviceIndex, 
        int computeUnits,
        int tilesInPart,
        int hwSideLen,
        int iterations,
        int runs, 
        int partMethod, 
        int verifiability, 
        int verbosity) {
    
    // Start: Matrix parsing region

    auto start = std::chrono::high_resolution_clock::now();
    bool read;
    auto matA = ReadMatrixCSR<T>(matrixFile, read);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = end - start;

    if (!read) {
        std::cout<< "Error: can not read the matrix file: " << matrixFile << std::endl;
        return EXIT_FAILURE;
    }
    
    std::cout<< "parsing_matrix_time (sec): " << time.count() << std::endl;
    
    if (verbosity&1){
        std::cout << "matA->nnz(): " << matA->nnz() <<  std::endl;
        std::cout << "matA->rows(): " << matA->rows() <<  std::endl;
        std::cout << "matA->cols(): " << matA->cols() <<  std::endl;
    }

    // End: Matrix parsing region

    // Start: Partitioning region

    int yParts = computeUnits;
    int xParts = std::ceil(matA->cols()/(double)hwSideLen);/*tiles_in_part*/;

    std::cout << "tilesInYPart: " << xParts <<  std::endl;

    if (hwSideLen < std::ceil(matA->rows()/(double)yParts)) {
        std::cout<< "The hardware size: " << hwSideLen 
            <<  " can not accomodate y_partition size: " << matA->rows()/yParts << std::endl;
        return EXIT_FAILURE;
    }

    // x is now as many times partitioned
    if (hwSideLen < std::ceil(matA->cols()/xParts)) {
        std::cout<< "The hardware size: " << hwSideLen 
            <<  " can not accomodate x_partition size: " << matA->cols()/xParts << std::endl;
        return EXIT_FAILURE;
    }

    start = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<CSRMatrix<T>*>> tiles(yParts);
    for (int i=0; i<yParts; i++) {
        tiles[i].reserve(xParts);
    }

    auto yPartRows = std::vector<std::vector<int>>(yParts);
    switch (partMethod) { // TODO: Enum conversion here and other places
        case 2: // Row-shuffle for balanced nnz per y_partition tiling
            PartitionMatrixIntoNnzBalancedYPartitionTiles(*matA, matA->rows(), matA->cols(),
                yParts, xParts, tiles, yPartRows);
            break;
        default: std::cout<< "Invalid partitioning method specified" << std::endl;
            return EXIT_FAILURE;
    }
    
    // End: Partitioning region

    end = std::chrono::high_resolution_clock::now();
    time = end - start;
    std::cout<< "partitioning_matrix_time (sec): " << time.count() << std::endl;
    
    if (verifiability&2) {
        verfiyTilePartitioningSpmv(*matA, yParts, xParts, 1, tiles, yPartRows, partMethod);
    }

    // SpMV vectors
    auto vecX = DenseVector<T>(matA->cols()); // Ax=b
    auto vecB = DenseVector<T>(matA->cols()); // Ax=b (fpga)
    
    // std::unique_ptr<CSRMatrix<double>> matA_db = readMatrixCSR<double>(matrixFile, read);
    CSRMatrix<double> matA_db(matA->nnz(), matA->rows(), matA->cols());
    for (int i=0; i<matA->nnz(); i++) {
        matA_db.setData(i, matA->getData(i));
        matA_db.setColIndex(i, matA->getColIndex(i));
    }

    for (int i=0; i<matA->rows(); i++){
        matA_db.setRowPointer(i, matA->getRowPointer(i));
    }

    float min = -10.0f;
    float max = 10.0f;
    srand(0);
    std::generate(vecX.elements.get(), vecX.elements.get()+vecX.size(), 
        [&min, &max](){
            float scale = (float)rand()/(float)RAND_MAX;
            return min + scale * (max-min);
        });
    
    auto vecC = DenseVector<T>(matA->cols(), 0); // Ax=c (ref)
    TiledMatrixVectorMult<T>(tiles, yParts, xParts, vecX, vecC, yPartRows, partMethod);

    // Start: Device and kernels creation
    auto device = xrt::device(deviceIndex);
    auto uuid = device.load_xclbin(binaryFile);

    std::vector<xrt::kernel> spmvKrnl1(tiles.size()), 
                            spmvKrnl2(tiles.size()),
                            spmvKrnl3(tiles.size()), 
                            spmvKrnl4(tiles.size());

    CreateKernels(spmvKrnl1, spmvKrnl2, spmvKrnl3, spmvKrnl4, 
        device, uuid, binaryFile, tiles.size(), verbosity);

    // End: Device and kernels creation

    // Start: Device buffer creation and assignment

    std::vector<uint*> boIndicesMaps(tiles.size());
    std::vector<T*> boValuesMaps(tiles.size());

    std::vector<xrt::bo> boIndices(tiles.size()),
                            boValues(tiles.size()); 
                            //  bo_nnz_blks(tiles.size());
    std::vector<uint> validTiles;
    validTiles.reserve(tiles.size());

    AllocateBuffers(device, spmvKrnl1, boIndices, boValues, tiles, validTiles, BLOCK_SIZE);

    // Each title's value count
    std::vector<uint> nnzBlocksTot; 
    nnzBlocksTot.reserve(tiles.size());
    std::vector<uint> rowBlocksTot; 
    rowBlocksTot.reserve(tiles.size());
    std::vector<uint> vecBlocksTot; 
    vecBlocksTot.reserve(tiles.size());

    // For now they are fixed across all the partitions
    uint vecBlocks = ((tiles[0][0]->cols()-1)/BLOCK_SIZE)+1;
    uint rowBlocks = ((tiles[0][0]->rows())/BLOCK_SIZE)+1; // |row_ptr|=|x|+1

    for (int i=0; i<tiles.size(); i++) {
        uint locVecBlocks = ((tiles[i][0]->cols()-1)/BLOCK_SIZE)+1;
        uint locRowBlocks = ((tiles[i][0]->rows())/BLOCK_SIZE)+1; // |row_ptr|=|x|+1
        vecBlocks = locVecBlocks > vecBlocks ? locVecBlocks : vecBlocks;
        rowBlocks = locRowBlocks > rowBlocks ? locRowBlocks : rowBlocks;
    }   

    // TODO: Skip packing for empty tiles
    PackTilesIntoBuffers(boIndices, boValues, tiles, vecX,
        nnzBlocksTot, rowBlocksTot, vecBlocksTot, validTiles, vecBlocks, rowBlocks, BLOCK_SIZE);

    if (verifiability&2) {
         // TODO: add the sparse tile skipping logic in here.
        VerifyTilesPacking(boIndices, boValues, tiles, validTiles, rowBlocks, vecBlocks, BLOCK_SIZE);
    }

    // Sync. buffers to FPGA
    for (int i=0; i<tiles.size(); i++) {
        boValues[i].sync(XCL_BO_SYNC_BO_TO_DEVICE);
        boIndices[i].sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    // End: Device buffer creation and assignment

    std::chrono::duration<double> totalKernelTime(0);
    std::chrono::duration<double> lowestKernelTime(0);

    int lowestIndex = 0;

    std::vector<xrt::run> runKrnl1(tiles.size()), 
                        runKrnl2(tiles.size()),
                        runKrnl3(tiles.size()), 
                        runKrnl4(tiles.size());

    for (uint i=0; i<runs; i++) {

        std::cout<< "starting kernels..." << std::endl;
        for (int j=0; j<tiles.size(); j++) {
            runKrnl1[j] = xrt::run(spmvKrnl1[j]);
            runKrnl1[j].set_arg(3, boValues[j]); 
            runKrnl1[j].set_arg(4, boIndices[j]);
            runKrnl1[j].set_arg(5, vecBlocksTot[j]); // TODO: do the total calculation above
            runKrnl1[j].set_arg(6, rowBlocksTot[j]); // TODO: do the total calculation above
            runKrnl1[j].set_arg(7, vecBlocks); // vecBlock constant across all the tiles
            runKrnl1[j].set_arg(8, nnzBlocksTot[j]); // TODO: do the total calculation above
            runKrnl1[j].set_arg(9, iterations); 

            runKrnl2[j] = xrt::run(spmvKrnl2[j]);
            runKrnl2[j].set_arg(4, vecBlocks); // vecBlock constant across all the tiles
            runKrnl2[j].set_arg(5, rowBlocks);
            runKrnl2[j].set_arg(6, tiles[j][0]->rows());
            runKrnl2[j].set_arg(7, validTiles[j]); //
            runKrnl2[j].set_arg(8, nnzBlocksTot[j]); // TODO: do the total calculation above
            runKrnl2[j].set_arg(9, iterations);

            runKrnl3[j] = xrt::run(spmvKrnl3[j]);
            runKrnl3[j].set_arg(3, validTiles[j]); 
            runKrnl3[j].set_arg(4, nnzBlocksTot[j]); // TODO: do the total calculation above
            runKrnl3[j].set_arg(5, iterations); 

            runKrnl4[j] = xrt::run(spmvKrnl4[j]);
            runKrnl4[j].set_arg(2, vecBlocks); // vecBlock constant across all the tiles
            runKrnl4[j].set_arg(3, validTiles[j]);
            runKrnl4[j].set_arg(4, iterations); 

        }

        for (int j=0; j<tiles.size(); j++) {
            runKrnl2[j].start();
            runKrnl3[j].start();
            runKrnl4[j].start();    
        }

        auto kernel_start = std::chrono::high_resolution_clock::now();

        for (int j=0; j<tiles.size(); j++) {
            runKrnl1[j].start();
        }

        for (int j=0; j<tiles.size(); j++) {
            runKrnl1[j].wait();
        }

        auto kernelEnd = std::chrono::high_resolution_clock::now();
        auto kernelTime = std::chrono::duration<double>(kernelEnd - kernel_start);

        for (int j=0; j<tiles.size(); j++) {
            runKrnl2[j].wait();
            runKrnl3[j].wait();
            runKrnl4[j].wait();
        }
        
        if (i == 0)
            lowestKernelTime = kernelTime;
        
        if (lowestKernelTime > kernelTime) {
            lowestKernelTime = kernelTime;
            lowestIndex = i;
        }

        totalKernelTime += kernelTime;
    }
    
    uint transBlocks = 0;
    for (int i=0; i<tiles.size(); i++) {
        transBlocks += nnzBlocksTot[i] * 2; // col indices + nnz vals
        transBlocks += rowBlocksTot[i] + rowBlocks; // row pointers + result y partition
        transBlocks += vecBlocksTot[i]; // vector x partition
    }

    auto transBytes = transBlocks * BLOCK_SIZE * sizeof(int);
    double transferGB = (double) transBytes / ((double)  1024*1024*1024);

    std::cout<< "kernel_lowest_running time (µsec): " 
        << std::chrono::duration_cast<std::chrono::microseconds>(lowestKernelTime).count() << std::endl;
    std::cout<< "kernel_running_time (µsec, avg of " << runs << " runs): " 
        << std::chrono::duration_cast<std::chrono::microseconds>(totalKernelTime).count()/runs << std::endl;

    std::cout<< "data_transfer_per_run_(MiB): " << transferGB*1024 << std::endl;
    std::cout<< "effective_bandwidth (GiB/Sec): " << (transferGB*runs*iterations) / (double) totalKernelTime.count() << std::endl;
    std::cout<< "highest_effective_bandwidth (GiB/Sec): " << (transferGB*iterations) / (double) lowestKernelTime.count() << std::endl;

    double flops = matA->nnz() * 2;
    double gflops = flops / (1000 * 1000 * 1000);
    std::cout<< "effective_GFLOPS (upper-bound): " << (gflops*runs*iterations) / (double) totalKernelTime.count() << std::endl;
    std::cout<< "highest_effective_GFLOPS (upper-bound): " << (gflops*iterations) / (double) lowestKernelTime.count() << std::endl;

    for (int i=0; i<tiles.size(); i++) {
        boValues[i].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    int locRows = 0;
    for (int i=0; i<tiles.size(); i++) {
        auto bo_vals_map = boValues[i].map<T*>();
        auto offset = nnzBlocksTot[i] + vecBlocksTot[i];
        offset *= BLOCK_SIZE;
        for (int j=0; j<tiles[i][0]->rows(); j++) { 
            int index = partMethod == 1 ? locRows+j : yPartRows[i][j];
            vecB[index] = bo_vals_map[j+offset];
        }
        locRows += tiles[i][0]->rows();
    }

    float tol = 1 / (double) std::pow(10, 4);
    int mismatchs = 0;
    for (int mm = 0; mm < vecB.size(); ++mm) {
        double v_cpu_sn = vecC[mm];
        double v_fpga = vecB[mm];
        double dff_sn = fabs(v_cpu_sn - v_fpga);
        double x_sn = std::min(fabs(v_cpu_sn), fabs(v_fpga)) + tol;
        bool scl_dff_fail_sn = dff_sn/x_sn > tol;
        bool abs_diff_fail_sn = dff_sn > tol;
        mismatchs += scl_dff_fail_sn && abs_diff_fail_sn;
    }
    float diffpercent = 100.0 * mismatchs / vecB.size();
    bool pass = diffpercent < 0.0;
    if(pass){
        std::cout << "Validation success\n";
    } else{
        std::cout << "Validation failed\n";
        std::cout<< std::fixed << std::setprecision(6) << "errors, tol = " << tol << ", num_mismatch = " << mismatchs << " , percent = " <<  diffpercent << std::endl;
    }
    return 0;
}

int main(int argc, char** argv) {

    if (argc != 11) { // TODO: Support optional args 
        std::cout << "Arguments: " << argc << std::endl;
        std::cout << "Usage: " << argv[0] << " <XCLBIN File> <Matrix File> <Device Id> <Test type> " 
            << "<CU Count> <Tiles in Part.> <HW Size> <CSR Part. Method> <Iterations> <Runs> " << std::endl;
        std::cout << "      <Test Type>: 1 = CSR SpMV on FPGA (4 kernel group replicated multi-tile)" << std::endl;
        std::cout << "      <CSR Part. Method>: 1 = Static spatial bounds  distribution" << std::endl;
        std::cout << "      <CSR Part. Method>: 2 = Balanced rows/nnz per partition and static spatial bounds colum distribution" << std::endl;
        std::cout << "      <CSR Part. Method>: 3 = Balanced rows/nnz per partition and col-shuffle to pack tiles denser; left-to-right" << std::endl;

        return EXIT_FAILURE;
    }

    // TODO: args into a struct
    std::string binaryFile = argv[1];
    std::cout << "binaryFile: " << binaryFile << std::endl;

    std::string matrixFile = argv[2];
    std::cout << "matrixFile: " << matrixFile << std::endl;

    int deviceIndex = std::stoi(argv[3]);
    std::cout << "deviceIndex: " << deviceIndex << std::endl;

    int testType = std::stoi(argv[4]);
    std::cout << "testType: " << testType << std::endl;

    int computeUnits = std::stoi(argv[5]);
    std::cout << "computeUnits: " << computeUnits << std::endl;

    int tilesInPart = std::stoi(argv[6]);
    std::cout << "tilesInPart: " << tilesInPart << std::endl;

    int hwSideLen = std::stoi(argv[7]);

    int partMethod = std::stoi(argv[8]); // Todo: convert to enum
    std::cout << "partMethod: " << partMethod << std::endl;

    int iterations = std::stoi(argv[9]);
    std::cout << "iterations: " << iterations << std::endl;

    int runs = std::stoi(argv[10]);
    std::cout << "runs: " << runs << std::endl;

    int verifiability = 0, // Todo: convert to enum
        verbosity = 1;

    switch (testType) { 
        case 0: 
            return RunHiHiSpMV<float>(binaryFile, matrixFile, deviceIndex, 
                        computeUnits, tilesInPart, hwSideLen, iterations, runs, partMethod, verifiability, verbosity); 
            break;
        default: // Other test calls can be incoporated if needed           
            std::cout << "<Test type>: " << testType << " is not defined." << std::endl;
            return EXIT_FAILURE;
    }
}
