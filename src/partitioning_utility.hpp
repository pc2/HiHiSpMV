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
#include <algorithm>
#include <random>

#include "../include/includes.hpp"
#include "../include/dense_vector.hpp"
#include "../include/csr_matrix.hpp"
#include "../include/index_value_pair.hpp"

template <typename T> 
static inline void PartitionMatrixIntoNnzBalancedYPartitionTiles(
    const CSRMatrix<T> &source,
    const int srcRows,
    const int srcCols,
    const int yParts, 
    const int xParts,
    std::vector<std::vector<CSRMatrix<T>*>> &tiles,
    std::vector<std::vector<int>> &yPartRows) { 
    
    // std::cout<< "srcRows: " << srcRows << std::endl;

    int xPartRem = static_cast<int>(srcCols) % xParts == 0 ? 0 : 1;
    int xPartSize = static_cast<int>(srcCols) / xParts + xPartRem; // Size of x each partition

    std::cout<< "xParts:" << xParts << std::endl;
    std::cout<< "xPartSize:" << xPartSize << std::endl;

    auto nnzBlocks = std::vector<std::pair<int, int>>(srcRows);
    for (int i=0; i<nnzBlocks.size(); i++) {
        nnzBlocks[i].first = (source.getRowPointer(i+1) - source.getRowPointer(i));
        nnzBlocks[i].second = i;
    }

    std::sort(nnzBlocks.begin(), nnzBlocks.end(), [](auto &left, auto &right) {
            return left.first < right.first;
    });

    // std::reverse(nnzBlocks.begin(), nnzBlocks.end());

    auto yPartNnz = std::vector<int>(yParts);

    bool frwd = true;
    for (int i=0; i<source.rows(); i+=yParts, frwd=!frwd) {
        for (int j=frwd?0:yParts-1, k=i; (k<i+yParts && k<source.rows()); frwd?j++:j--, k++) {
            yPartNnz[j] += nnzBlocks[k].first;
            yPartRows[j].push_back(nnzBlocks[k].second);  
        }
    }
    
    // auto yPartRows_sorted = std::vector<std::vector<std::pair<double, int>>>();

    // for (std::vector<int>& vector : yPartRows) {
    //     auto row_abs_sum = std::vector<std::pair<double, int>>(vector.size());
    //     for (int i=0; i<vector.size(); i++) {
    //         int row = vector[i];
    //         double abs_sum = 0;
    //         auto first = source.getRowPointer(row);
    //         auto last = source.getRowPointer(row+1);
    //         for (int j=first; j<last; j++) { // Count each cols entries
    //             abs_sum += abs(source.getData(j));
    //         }
    //         row_abs_sum[i].first = abs_sum;
    //         row_abs_sum[i].second = row;
    //     }
    //     std::sort(row_abs_sum.begin(), row_abs_sum.end(), 
    //         [](auto &left, auto &right) {
    //             return left.first < right.first;
    //         });
    //     yPartRows_sorted.push_back(row_abs_sum);
    // }

    // for (int i=0; i<yPartRows.size(); i++) {
    //     for (int j=0; j<yPartRows[i].size(); j++) {
    //         // std::cout<< "abs-value:" << yPartRows_sorted[i][j].first << std::endl;
    //         yPartRows[i][j] = yPartRows_sorted[i][j].second;
    //     }
    // }


    for (int i=0; i<yParts; i++) {
        // Populate the CSC matrix according to rows ids
        // std::cout << "yPartNnz[" << i <<"]: " << yPartNnz[i] <<  std::endl;

        auto cscPart = CSCMatrix<T>(yPartNnz[i], srcCols);
        cscPart.clear();
        for (auto row : yPartRows[i]) { // Row id wise iterate
            auto first = source.getRowPointer(row);
            auto last = source.getRowPointer(row+1);
            for (int j=first; j<last; j++) { // Count each cols entries
                auto col = source.getColIndex(j);
                cscPart.setColPointer(col, cscPart.getColPointer(col)+1);
            }
        }
        for (int j=0, prefixSum=0; j<cscPart.cols()+1; j++) { // Prefix-sum col ptr
            auto old = cscPart.getColPointer(j);
            cscPart.setColPointer(j, prefixSum);
            prefixSum += old;
        }
        // auto rd = std::random_device {}; 
        // auto rng = std::default_random_engine { rd() };

        auto k=0; // k = local row index
        for (auto row : yPartRows[i]) { 
            // Random shuffle
            // int cols = source.getRowPointer(row+1)-source.getRowPointer(row);
            // std::vector<int> random_cols(cols);
            // for (int j=0; j<cols; j++) {
            //     random_cols[j] = j;
            // }
            // std::shuffle(random_cols.begin(), random_cols.end(), rng);
            // for (int j=0; j<cols; j++) {
            //     std::cout << random_cols[j] << "," ;
            // }
            // std::cout << std::endl;

            /*
            for (int j=source.getRowPointer(row); j<source.getRowPointer(row+1); j++) {
                auto col = source.getColIndex(j);
                auto dest = cscPart.getColPointer(col);
                cscPart.setRowIndex(dest, k); // localized row index
                cscPart.setData(dest, source.getData(j));
                cscPart.setColPointer(col, dest+1);
            }
            k++;            
            */

            for (int j=source.getRowPointer(row), l=0; j<source.getRowPointer(row+1); j++, l++) {
                auto col = source.getColIndex(j); // random: source.getRowPointer(row)+random_cols[l]
                auto dest = cscPart.getColPointer(col);
                cscPart.setRowIndex(dest, k); // localized row index
                cscPart.setData(dest, source.getData(j)); // random: source.getRowPointer(row)+random_cols[l]
                cscPart.setColPointer(col, dest+1);
            }
            k++;            
        }
        for (int j=0, k=0; j<cscPart.cols()+1; j++) { // 1 index left-shift col ptr
            auto old = cscPart.getColPointer(j);
            cscPart.setColPointer(j, k);
            k = old;
        }

        // Convert the CSC partition to CSR tiles
        for (int j=0; j<xParts; j++) {
            int xPartStart = j*xPartSize; // Inclusive
            int xPartEnd = xPartStart + xPartSize; // Exclusive
            if (j == xParts-1) { xPartEnd -= (xPartEnd - srcCols); }

            auto xFirst = cscPart.getColPointer(xPartStart);
            auto xLast = cscPart.getColPointer(xPartEnd);
            auto xNnz = xLast-xFirst;

            // std::cout << "x_tile: " << j << std::endl;
            // std::cout << "xPartStart: " << xPartStart <<  std::endl;
            // std::cout << "xPartEnd: " << xPartEnd <<  std::endl;
            // std::cout << "xFirst: " << xFirst <<  std::endl;
            // std::cout << "xLast: " << xLast <<  std::endl;
            // std::cout << "xNnz: " << xNnz <<  std::endl;

            // Convert the current partition to CSR matrix
            auto csrTile = new CSRMatrix<T>(xNnz, yPartRows[i].size(), xPartEnd-xPartStart); // nnz, rows, cols
            csrTile->clear();

            for (int k=xFirst; k<xLast; ++k) { // Count each row's entries
                auto row = cscPart.getRowIndex(k);
                csrTile->setRowPointer(row, csrTile->getRowPointer(row)+1);
            }

            for (int k=0, l=0; k<csrTile->rows()+1; ++k) { // Prefix-sum row ptr
                auto old = csrTile->getRowPointer(k);
                csrTile->setRowPointer(k, l);
                l += old;
            }

            for (int k=xPartStart, l=0, m=0; k<xPartEnd; ++k, l++) { // k = local col index, l = local nnz index
                for (int n=cscPart.getColPointer(k); n<cscPart.getColPointer(k+1); n++, m++) {
                    auto row = cscPart.getRowIndex(n);
                    auto dest = csrTile->getRowPointer(row);
                    csrTile->setColIndex(dest, l);
                    csrTile->setData(dest, cscPart.getData(n));
                    csrTile->setRowPointer(row, dest+1);             
                }            
            }

            for (int k=0, l=0; k<csrTile->rows()+1; ++k) { // 1 index left-shift row ptr
                auto old = csrTile->getRowPointer(k);
                csrTile->setRowPointer(k, l);
                l = old;
            }

            // std::cout<< "CSR Matrix tile:" << std::endl << *csrTile << std::endl;
            tiles[i].push_back(csrTile);
        }       
        // std::cout<< "tiles[i].size():" << tiles[i].size() << std::endl;
    }

}

template<typename T> 
void TiledMatrixVectorMult(
        std::vector<std::vector<CSRMatrix<T>*>> &tiles,
        const int yParts, 
        const int xParts,
        const DenseVector<T> &vec,
        DenseVector<T> &vecResComb,
        const std::vector<std::vector<int>> &yPartRows,
        const int part_method) {
    
    auto vecParts = std::vector<DenseVector<T>*>();
    vecParts.reserve(xParts);
    for (int i=0; i<xParts; i++) {
        auto vecPart = new DenseVector<T>(tiles[0][i]->cols());
        // Offset determined by cols in the first tile
        std::copy(vec.elements.get()+tiles[0][0]->cols()*i, 
            vec.elements.get()+tiles[0][0]->cols()*i+vecPart->size(), vecPart->elements.get());
        vecParts.push_back(vecPart);
    }

    auto resParts = std::vector<DenseVector<T>*>();
    resParts.reserve(xParts);
    for (int i=0; i<yParts; i++) {
        auto resPart = new DenseVector<T>(tiles[i][0]->rows());
        resParts.push_back(resPart);
    }

    for (int i=0; i<yParts; i++) { // Multi-thread it, if slow
        for (int j=0; j<xParts; j++) {
            matrixVectorMult<T>(*tiles[i][j], *vecParts[j], *resParts[i]);
        }
    }

    auto rowIndices = yPartRows;
    auto rows = 0;
    for (int i=0; i<yParts; i++) { // Multi-thread it, if slow
        auto resPart = resParts[i];
        for (int j=0; j<resPart->size(); j++) { // Multi-thread it, if slow
            int index = part_method > 1 ? rowIndices[i][j] : rows+j; 
            vecResComb[index] = resPart->at(j);
        }
        rows += resPart->size();
    }

}

template<typename T>
void verfiyTilePartitioningSpmv(
        const CSRMatrix<T> &matA,
        const int yParts, 
        const int xParts,
        const double iota,
        std::vector<std::vector<CSRMatrix<T>*>> &tiles, 
        std::vector<std::vector<int>> &yPartRows,
        int part_method) {

    auto vec = DenseVector<T>(matA.rows());
    std::iota(vec.elements.get(), vec.elements.get()+vec.size(), iota);

    std::ofstream myfile;

    auto vecRes = DenseVector<T>(matA.rows());
    matrixVectorMult<T>(matA, vec, vecRes);
    
    auto vecParts = std::vector<DenseVector<T>*>();
    vecParts.reserve(xParts);
    for (int i=0; i<xParts; i++) {
        auto vecPart = new DenseVector<T>(tiles[0][i]->cols());
        // Offset determined by cols in the first tile
        std::copy(vec.elements.get()+tiles[0][0]->cols()*i, 
            vec.elements.get()+tiles[0][0]->cols()*i+vecPart->size(), vecPart->elements.get());
        vecParts.push_back(vecPart);

        // auto name = "vecParts_" + std::to_string(i) + ".txt";
        // myfile.open(name);
        // for (int i=0; i<vecPart->size(); i++) {
        //     myfile<< "vecPart["<<i<<"]: " << vecPart->at(i) << std::endl;
        // }
        // myfile.close();
    }

    auto resParts = std::vector<DenseVector<T>*>();
    resParts.reserve(xParts);
    for (int i=0; i<yParts; i++) {
        auto resPart = new DenseVector<T>(tiles[i][0]->rows());
        resParts.push_back(resPart);
    }

    for (int i=0; i<yParts; i++) { // Multi-thread it, if slow
        for (int j=0; j<xParts; j++) {
            matrixVectorMult<T>(*tiles[i][j], *vecParts[j], *resParts[i]);
        }
        // std::cout << "norm of partial: " << vectorNorm(*resParts[i]) << std::endl;

        // auto name = "resParts_" + std::to_string(i) + ".txt";
        // myfile.open(name);
        // // auto resPart = resParts[i];
        // for (int j=0; j<resParts[i]->size(); j++) {
        //     myfile<< "resPart["<<j<<"]: " << resParts[i]->at(j) << std::endl;
        // }
        // myfile.close();
    }

    auto vecResComb = DenseVector<T>(matA.rows());

    
    auto rowIndices = yPartRows;
    auto rows = 0;
    for (int i=0; i<yParts; i++) { // Multi-thread it, if slow
        auto resPart = resParts[i];
        for (int j=0; j<resPart->size(); j++) { // Multi-thread it, if slow
            int index = part_method > 1 ? rowIndices[i][j] : rows+j; 
            vecResComb[index] = resPart->at(j);
        }
        rows += resPart->size();
    }


    // myfile.open("yPartRows.txt");
    // for (auto part : yPartRows) {
    //     myfile << "partition_starts:" << std::endl ;
    //     for (auto row : part) {
    //         myfile << row << std::endl;
    //     }
    // }
    // myfile.close();
    
    // myfile.open("vec_ref_res.txt");
    //   for (int i=0; i<vecRes.size(); i++) {
    //     myfile<< "vec_ref_res["<<i<<"]: " << vecRes[i] << std::endl;
    // }
    // myfile.close();

    // myfile.open("vecResComb.txt");
    //   for (int i=0; i<vecResComb.size(); i++) {
    //     myfile<< "vecResComb["<<i<<"]: " << vecResComb[i] << std::endl;
    // }
    // myfile.close();

    auto equality = vectorNorm(vecRes) == vectorNorm(vecResComb);
    std::cout << "verfiyTilePartitioningSpmv()" << std::endl << "norm equality check: " << equality << std::endl;
    if (!equality) {
        std::cout << "norm of vecRes: " << vectorNorm(vecRes);
        std::cout << " and norm of vecResComb: " << vectorNorm(vecResComb) << std::endl;
    }
}