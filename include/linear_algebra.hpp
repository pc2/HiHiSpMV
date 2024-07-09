#pragma once

#include "includes.hpp"
#include "dense_vector.hpp"
#include "csr_matrix.hpp"
#include "csc_matrix.hpp"

#include <iterator>
#include <math.h>

template<typename T>
static inline double vectorDotProduct(const DenseVector<T> &vec1, const DenseVector<T> &vec2) {
    double res = 0.0;
    for (int index=0; index<vec1.size(); index++) {
        res += vec1[index] * vec2[index];
    }
    return res;
}

template<typename T>
static inline double vectorNorm(const DenseVector<T> &vec) {
   return sqrt(vectorDotProduct(vec, vec));
}

template<typename T>
static inline void matrixVectorMult(const CSRMatrix<T> &mat, const DenseVector<T> &vec, DenseVector<T> &vec_res) {
    for (int row=0; row<mat.rows(); row++) 
        for (int pointer=mat.getRowPointer(row); pointer<mat.getRowPointer(row+1); pointer++)
            vec_res[row] += mat.getData(pointer) * vec[mat.getColIndex(pointer)];
}