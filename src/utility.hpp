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

#include "../include/includes.hpp"
#include "../include/dense_vector.hpp"
#include "../include/csr_matrix.hpp"
#include "../include/index_value_pair.hpp"
#include "../include/csc_matrix.hpp"

#include <string.h>
template<typename T> 
std::ostream & operator<<(std::ostream &os, const DenseVector<T>& vec) {
    for (int i=0;i<vec.size();i++) {
        os << vec.at(i);
        if (i<vec.size()-1) {
            os << ", ";
        }
    }
    return os;
}

template<typename T> 
std::ostream & operator<<(std::ostream &os, const CSRMatrix<T>& mat) {
    os << "Row pointers: ";
    for (int row=0; row<=mat.rows(); row++) {
        os << mat.getRowPointer(row) << ", ";
    }
    os << std::endl;

    os << "Col indices: ";
    for (int col=0; col<mat.nnz(); col++) {
        os << mat.getColIndex(col) << ", ";
    }
    os << std::endl;

    os << "Data: ";
    for (int dat=0; dat<mat.nnz(); dat++) {
        os << mat.getData(dat) << ", ";
    }
    return os;
}

template<typename T> 
std::ostream & operator<<(std::ostream &os, const CSCMatrix<T>& mat) {
    os << "Col pointers: ";
    for (int col=0; col<=mat.cols(); col++) {
        os << mat.getColPointer(col) << ", ";
    }
    os << std::endl;

    os << "Row Indices: ";
    for (int row=0; row<mat.nnz(); row++) {
        os << mat.getRowIndex(row) << ", ";
    }
    os << std::endl;

    os << "Data: ";
    for (int dat=0; dat<mat.nnz(); dat++) {
        os << mat.getData(dat) << ", ";
    }
    return os;
}

template<typename T>
static inline bool readIntIntReal(std::string line, int &a, int &b, T &real) {
    std::istringstream lineStream(line);
    lineStream >> a >> b >> real;
    return !lineStream.fail();
}

template<typename T>
static inline bool readIntIntInt(std::string line, int &a, int &b, T &c) {
    std::istringstream lineStream(line);
    lineStream >> a >> b >> c;
    return !lineStream.fail();
}


template<typename T>
static inline std::unique_ptr<CSRMatrix<T>> ReadMatrixCSR(const std::string filePath, bool &read) {
    std::string comment = "%";
    std::unique_ptr<CSRMatrix<T>> matrix;
    std::ifstream file(filePath);

    if ((read = file.good())) {
        bool firstLine = true; 
        std::string line;
        int row, col; 
        T value;
        int currDataPtr = 0;
        
        while (std::getline(file, line)) {
            if (line.compare(0, comment.length(), comment) == 0) { 
                continue; 
            } else if (firstLine) {
                int nnz, rows, cols;
                if (!(read = readIntIntInt(line, rows, cols, nnz))) break;
                matrix.reset(new CSRMatrix<T>(nnz, rows, cols));
                for (int i=0;i<matrix->rows()+1;i++) {
                    matrix->setRowPointer(i, -1); // Todo: use std::copy() or fill()
                }
                firstLine = false;
            } else {
                if ((read = readIntIntReal(line, row, col, value))) {  
                    // if (value != f_value) std::cout<< "WARNING...." << std::endl;
                    if (matrix->getRowPointer(row-1) == -1) {
                        matrix->setRowPointer(row-1, currDataPtr);
                    }
                    matrix->setRowPointer(row, currDataPtr+1);
                    matrix->setData(currDataPtr, value);
                    matrix->setColIndex(currDataPtr, col-1); // Matrix Market matrices are 1 index based.
                    currDataPtr++;
                } else { //Last line
                    // forward pass to fill the gaps in rowPointer
                    int lastValidIndex = matrix->rowPointer[0];
                    for (int i=0;i<matrix->rows();i++) {
                        if ((i != 0 && matrix->getRowPointer(i) < matrix->getRowPointer(i-1)) ||
                            matrix->getRowPointer(i) == -1)
                            matrix->setRowPointer(i, lastValidIndex);
                        else
                            lastValidIndex = matrix->getRowPointer(i);
                    }

                    // backward pass to fill the gaps in rowPointer
                    lastValidIndex = matrix->getRowPointer(matrix->rows());
                    for (int i=matrix->rows();i>-1;i--) {
                        if (matrix->getRowPointer(i) == -1)
                            matrix->setRowPointer(i, lastValidIndex);
                        else
                            lastValidIndex = matrix->getRowPointer(i);
                    }
                    break;
                }
            }
        };
    } else {
        std::cout << "Could not open " << filePath << std ::endl;
    }
    return matrix;
}