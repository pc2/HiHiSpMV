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

#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <limits>

#include "../include/includes.hpp"
#include "../include/dense_vector.hpp"
#include "../include/csr_matrix.hpp"
#include "../include/index_value_pair.hpp"

#include <xrt/xrt_device.h>
#include <experimental/xrt_xclbin.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>
#include "xclbin.h"

#define sw_emu  0
#define hw_emu  1
#define hw      2

void CreateKernels(
        std::vector<xrt::kernel> &spmvKrnl1, 
        std::vector<xrt::kernel> &spmvKrnl2,
        std::vector<xrt::kernel> &spmvKrnl3, 
        std::vector<xrt::kernel> &spmvKrnl4,
        xrt::device &device,
        xrt::uuid &uuid,
        std::string &binaryFile,
        int computeUnits,
        int verbosity) {

    std::string spmvKrnl = "csr_spmv_repl_";
    auto spmvKrnl1Id = spmvKrnl + "1", 
                    spmvKrnl2Id = spmvKrnl + "2", 
                    spmvKrnl3Id = spmvKrnl + "3", 
                    spmvKrnl4Id = spmvKrnl + "4";

    for (int i=0; i<computeUnits; i++) {
        auto cuNum = "_" + std::to_string(i + 1);
        auto x = spmvKrnl1[i] = xrt::kernel(device, uuid, spmvKrnl1Id + ":{" + spmvKrnl1Id + cuNum + "}", true);
        spmvKrnl2[i] = xrt::kernel(device, uuid, spmvKrnl2Id + ":{" + spmvKrnl2Id + cuNum + "}", true);
        spmvKrnl3[i] = xrt::kernel(device, uuid, spmvKrnl3Id + ":{" + spmvKrnl3Id + cuNum + "}", true);
        spmvKrnl4[i] = xrt::kernel(device, uuid, spmvKrnl4Id + ":{" + spmvKrnl4Id + cuNum + "}", true);
        
        if (verbosity&2) {
            std::cout << spmvKrnl1Id + ":{" + spmvKrnl1Id + cuNum + "}" <<  std::endl;
            std::cout << spmvKrnl2Id + ":{" + spmvKrnl2Id + cuNum + "}" <<  std::endl;
            std::cout << spmvKrnl3Id + ":{" + spmvKrnl3Id + cuNum + "}" <<  std::endl;
            std::cout << spmvKrnl4Id + ":{" + spmvKrnl4Id + cuNum + "}" <<  std::endl;
            std::cout << "krnl_1_" << i << ".group_id(3): " << x.group_id(3) <<  std::endl;
            std::cout << "krnl_1_" << i << ".group_id(4): " << x.group_id(4) <<  std::endl;
        }
    }

    auto bitstream =  xrt::xclbin(binaryFile);
    auto kernels = bitstream.get_kernels();

    if (verbosity&2) {
        for (auto kernel : kernels) {
            std::cout << "kernel_name: " << kernel.get_name() <<  std::endl;
            for (auto cu : kernel.get_cus()) {
                std::cout << "cu name: " << cu.get_name() <<  std::endl;
                for (auto arg : cu.get_args()) {
                    // std::cout << "arg.group_id: " << arg.group_id() <<  std::endl;
                    std::cout << "arg.port: " << arg.get_port() <<  std::endl;
                    std::cout << "arg.get_host_type: " << arg.get_host_type() <<  std::endl;
                    for (auto mem : arg.get_mems()) {
                        std::cout << "mem.get_type: " << (int) mem.get_type() <<  std::endl;
                        std::cout << "mem.get_tag: " << mem.get_tag() <<  std::endl;
                        std::cout << "mem.get_used: " << mem.get_used() <<  std::endl;
                        std::cout << "mem.get_index: " << mem.get_index() <<  std::endl;
                    }
                }
            }
        }

        auto mems = bitstream.get_mems();
        for (auto mem : mems) {
            std::cout << "mem.get_type: " << (int) mem.get_type() <<  std::endl;
            std::cout << "mem.get_tag: " << mem.get_tag() <<  std::endl;
            std::cout << "mem.get_used: " << mem.get_used() <<  std::endl;
            std::cout << "mem.get_index: " << mem.get_index() <<  std::endl;
        }
    }
}

template <typename T> 
void AllocateBuffers(
        xrt::device &device,
        std::vector<xrt::kernel> &spmvKrnl1,
        std::vector<xrt::bo> &boIndices,
        std::vector<xrt::bo> &boValues, 
        std::vector<std::vector<CSRMatrix<T>*>> &tiles,
        std::vector<uint> &validTiles,
        uint blockSize) {
    auto normalFlags =  xrt::bo::flags::normal;
    int pageSize = 4*1024;

    for (int i=0; i<tiles.size(); i++) {
        int valuesBytes = 0, // (x vector part + nnz vals)*tiles + result y part
            indicesBytes = 0,
            rowBlockBytesMax = 0; // (row ptr part + nnz cols)*tiles
        
        uint valid_tile=0;
        for (int j=0; j<tiles[i].size(); j++) {
            auto tile = tiles[i][j];
            auto tileNnz = tile->nnz();

            uint nnzBlocks = ((tiles[i][j]->nnz()-1)/blockSize)+1;
            uint rowBlocks = ((tiles[i][j]->rows())/blockSize)+1; // |row_ptr| = |vec|+1
            uint vecBlocks = ((tiles[i][j]->cols()-1)/blockSize)+1;
            uint valueBlockBytes = sizeof(T) * nnzBlocks * blockSize; //csr value bytes
            uint colBlockBytes = sizeof(int) * nnzBlocks * blockSize; //col data bytes
            uint rowBlockBytes = sizeof(int) * rowBlocks * blockSize; //csr row pointer size
            uint vecBlockBytes = sizeof(T)* vecBlocks * blockSize;

            // Max row blocks for y partition read-out
            rowBlockBytesMax = rowBlockBytes > rowBlockBytesMax ? rowBlockBytes : rowBlockBytesMax;
            valuesBytes += (((valueBlockBytes + 2*vecBlockBytes)/pageSize)+1)*pageSize; // page-size divisible
            indicesBytes += (((rowBlockBytes + colBlockBytes)/pageSize)+1)*pageSize; // page-size divisible
            tileNnz ? ++valid_tile:0;
        }
        validTiles[i] = valid_tile;

        valuesBytes += (((rowBlockBytesMax-1)/pageSize)+1)*pageSize; // result y part
        indicesBytes += pageSize; // nnz per tile

        boValues[i] = xrt::bo(device, valuesBytes, normalFlags, spmvKrnl1[i].group_id(3));
        boIndices[i] = xrt::bo(device, indicesBytes, normalFlags, spmvKrnl1[i].group_id(4)); 
        
        auto boValsMap = boValues[i].map<T*>(); 
        auto boIndicesMap = boIndices[i].map<int*>();

        std::fill(boValsMap, boValsMap+valuesBytes/sizeof(T), 0);
        std::fill(boIndicesMap, boIndicesMap+indicesBytes/sizeof(int), 0);
    }
}

// Note: For now each tile has maxVecBlocks and maxRowBlocks
template <typename T> 
void PackTilesIntoBuffers(
        std::vector<xrt::bo> &boIndices,
        std::vector<xrt::bo> &boValues, 
        std::vector<std::vector<CSRMatrix<T>*>> &tiles,
        DenseVector<T> &vecX,
        std::vector<uint> &nnzBlocksTot,
        std::vector<uint> &rowBlocksTot, 
        std::vector<uint> &vecBlocksTot,
        std::vector<uint> &validTiles,
        uint maxVecBlocks,
        uint maxRowBlocks,
        uint blockSize) {

    for (int i=0; i<tiles.size(); i++) {
        auto boValsMap = boValues[i].map<T*>(); 
        auto boIndicesMap = boIndices[i].map<int*>();

        nnzBlocksTot[i] = 0;
        rowBlocksTot[i] = 0;
        vecBlocksTot[i] = 0;

        uint valOffset = 0; // (x vector part + nnz vals)*tiles + result y part
        uint indOffset = 0; // (row ptr part + nnz cols)*tiles
        uint vecOffset = 0; // vecX offset       

        uint nnzTileBlks = ((validTiles[i]-1)/blockSize)+1;
        // rowBlocksTot[i] += nnzTileBlks; // hack: counted towards rows_blocks
        indOffset += 1*blockSize; // offset for nnzs in blocks

        uint valid_tile=0;
        for (int j=0; j<tiles[i].size(); j++) {
            auto tile = tiles[i][j];
            auto tileNnz = tile->nnz();
            uint nnzBlocks = tileNnz ? ((tileNnz-1)/blockSize)+1 : 0;
            uint vecBlocks = tileNnz ? maxVecBlocks /*((tile->cols()-1)/blockSize)+1*/ : 0;
            uint rowBlocks = tileNnz ? maxRowBlocks /*((tile->rows())/blockSize)+1*/ : 0; // |row_ptr| = |vec|+1
            
            // copy partition of x_vec into values
            auto copyCols = tileNnz ? tile->cols() : 0;
            std::copy(vecX.elements.get()+vecOffset, vecX.elements.get()+vecOffset+copyCols, boValsMap+valOffset);
            vecOffset += tile->cols(); // vecX partition size can vary between titles
            valOffset += vecBlocks*blockSize;

            // copy row pointer of the tile into indices
            auto copyRows = tileNnz ? + tile->rows()+1 : 0;
            std::copy(tile->rowPointer.get(), tile->rowPointer.get()+copyRows, boIndicesMap+indOffset);
            indOffset += rowBlocks*blockSize; // TODO: Convert it to incremental

            // copy nnz values the tile into values
            std::copy(tile->data.get(), tile->data.get()+tile->nnz(), boValsMap+valOffset);
            valOffset += nnzBlocks*blockSize;

            // copy nnz cols the tile into indices
            std::copy(tile->colIndex.get(), tile->colIndex.get()+tile->nnz(), boIndicesMap+indOffset);
            indOffset += nnzBlocks*blockSize;

            // total counts needed for the kernels
            nnzBlocksTot[i] += nnzBlocks; 
            rowBlocksTot[i] += rowBlocks;
            vecBlocksTot[i] += vecBlocks;

            // nnz count in the tile - only write for a valid tile
            tileNnz ? (boIndicesMap[valid_tile] = nnzBlocks) : 0; 
            tileNnz ? ++valid_tile:0;
        }
        validTiles[i] = valid_tile;
    }
}

template <typename T> 
void PackVecOnlyIntoBuffers(
        std::vector<xrt::bo> &boValues, 
        std::vector<std::vector<CSRMatrix<T>*>> &tiles,
        DenseVector<T> &vecX,
        uint maxVecBlocks,
        uint blockSize) {

    for (int i=0; i<tiles.size(); i++) {
        auto boValsMap = boValues[i].map<T*>(); 
        uint valOffset = 0; // (x vector part + nnz vals)*tiles + result y part
        uint vecOffset = 0; // vecX offset       

        uint valid_tile=0;
        for (int j=0; j<tiles[i].size(); j++) {
            auto tile = tiles[i][j];
            auto tileNnz = tile->nnz();
            uint nnzBlocks = tileNnz ? ((tileNnz-1)/blockSize)+1 : 0;
            uint vecBlocks = tileNnz ? maxVecBlocks /*((tile->cols()-1)/blockSize)+1*/ : 0;
            
            // copy partition of x_vec into values
            auto copyCols = tileNnz ? tile->cols() : 0;
            std::copy(vecX.elements.get()+vecOffset, vecX.elements.get()+vecOffset+copyCols, boValsMap+valOffset);

            vecOffset += tile->cols(); // vecX partition size can vary between titles
            valOffset += vecBlocks*blockSize;
            // copy nnz values the tile into values
            valOffset += nnzBlocks*blockSize;
        }
    }
}

template <typename T> 
void VerifyTilesPacking(
        std::vector<xrt::bo> &boIndices,
        std::vector<xrt::bo> &boValues, 
        std::vector<std::vector<CSRMatrix<T>*>> &tiles,
        std::vector<uint> &validTiles,
        uint maxRowBlocks, 
        uint maxVecBlocks, 
        uint blockSize) { 

    bool equality = true;
    for (int partInd=0; partInd<tiles.size(); partInd++){

        auto boValsMap = boValues[partInd].map<T*>(); 
        auto boIndicesMap = boIndices[partInd].map<int*>();
        auto yPart = DenseVector<T>(maxRowBlocks*blockSize, 0); // unpacking mult. result
        auto yRef = DenseVector<T>(maxRowBlocks*blockSize, 0); // ref mult. result

        uint valOffset = 0; // (x vector part + nnz vals)*tiles + result y part
        uint indOffset = 0; // (row ptr part + nnz cols)*tiles
        uint nnzTileBlks = ((validTiles[partInd]-1)/blockSize)+1;
        indOffset += nnzTileBlks*blockSize; // for nnzs in blocks

        for (int ind_tile=0; ind_tile<tiles[partInd].size(); ind_tile++) {
            // std::cout<< "tile: " << ind_tile << std::endl;
            int nnzBlocks = boIndicesMap[ind_tile]; // Read number of nnz in the current tile
            auto xPart = DenseVector<T>(maxVecBlocks*blockSize, 0); 
            auto rowPart = DenseVector<uint>(maxRowBlocks*blockSize, 0);

            // Read vector part.
            std::copy(boValsMap+valOffset, boValsMap+valOffset+maxVecBlocks*blockSize, xPart.elements.get());
            valOffset += maxVecBlocks*blockSize;

            // Read row_ptr part.
            std::copy(boIndicesMap+indOffset, boIndicesMap+indOffset+maxRowBlocks*blockSize, rowPart.elements.get());
            indOffset += maxRowBlocks*blockSize;

            // Read all the cols and nnzs for the row and multiply
            for (int row=0, z=0; row<tiles[partInd][0]->rows(); row++) {
                // std::cout<< "row: " << row << ", ";
                int nnzPacked = rowPart[row+1] - rowPart[row];
                int nnzOrig = tiles[partInd][ind_tile]->getRowPointer(row+1) - tiles[partInd][ind_tile]->getRowPointer(row);
                
                if (nnzPacked != nnzOrig)  {
                    std::cout<< "tile: " << partInd << ", " << ind_tile << ", row: " << row;
                    std::cout<< ", nnz count mismatch warning: " << nnzPacked << " vs. " << nnzOrig << std::endl;
                }
                
                for (int ind=rowPart[row]; ind<rowPart[row+1]; ind++, z++) {
                    if (boValsMap[ind+valOffset] != tiles[partInd][ind_tile]->getData(z))
                        std::cout<< "values mismatch warning: " << boValsMap[ind+valOffset] << " vs. " << tiles[partInd][ind_tile]->getData(z) << std::endl;
                    if (boIndicesMap[ind+indOffset] != tiles[partInd][ind_tile]->getColIndex(z)) 
                        std::cout<< "col mismatch warning: " << boIndicesMap[ind+indOffset] << " vs. " << tiles[partInd][ind_tile]->getColIndex(z) << std::endl;
                    yPart[row] += boValsMap[ind+valOffset] * xPart[boIndicesMap[ind+indOffset]];
                }
            }
            valOffset += nnzBlocks*blockSize;
            indOffset += nnzBlocks*blockSize;
            matrixVectorMult<T>(*tiles[partInd][ind_tile], xPart, yRef);
            equality &= vectorNorm(yPart) == vectorNorm(yRef);
            if (!equality) {
                std::cout<< "verifyTilesPacking()" << std::endl;
                std::cout<< "partInd: " << partInd << ", ind_tile: " << ind_tile <<", norm of partition equality: " << equality << std::endl;
                std::cout<< "vectorNorm(yPart): " << vectorNorm(yPart); 
                std::cout<< " and vectorNorm(yRef): " << vectorNorm(yRef) << std::endl;
                std::cout<< "yPart:" << yPart << std::endl;
                std::cout<< "yRef:" << yRef << std::endl;
            }
        }
    }
    std::cout<< "verifyTilesPacking(): norm of partition equality: " << equality << std::endl;
}