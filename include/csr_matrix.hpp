#pragma once

#include "includes.hpp"

template<typename T>
class CSRMatrix {
    
    private:
        uint nnz_, rows_, cols_;
    public:
        std::unique_ptr<T[]> data;
        std::unique_ptr<int[]> colIndex;
        std::unique_ptr<int[]> rowPointer;

        CSRMatrix<T>();
        CSRMatrix<T>(const uint nnz, const uint row, const uint cols = 0);
        CSRMatrix<T>(std::unique_ptr<T[]> &data, std::unique_ptr<int[]> &colIndex,
            std::unique_ptr<int[]> &rowPointer, const uint nnz, const uint rows, const uint cols = 0);

        CSRMatrix<T>(const CSRMatrix<T>& CSRMatrix);

        ~CSRMatrix<T>();

        const uint rows() const;
        const uint nnz() const;
        const uint cols() const;

        // T& operator()(const int x, const int y);
        // const T& operator()(const int x, const int y);

        void setData(const uint index, const T& value);
        const T& getData(uint index) const;

        void setColIndex(const uint index, const int value);
        const int getColIndex(uint index) const;

        void setRowPointer(const uint index, const int value);
        const int getRowPointer(uint index) const;

        void clear();
};

template <typename T> CSRMatrix<T>::CSRMatrix(): CSRMatrix(0, 0, 0){ }
template <typename T> CSRMatrix<T>::CSRMatrix(const uint nnz, const uint rows, const uint cols): 
    data(std::make_unique<T[]>(nnz)), colIndex(std::make_unique<int[]>(nnz)),
    rowPointer(std::make_unique<int[]>(rows+1)), nnz_(nnz), rows_(rows), cols_(cols) { }

template <typename T> CSRMatrix<T>::CSRMatrix(std::unique_ptr<T[]> &data, std::unique_ptr<int[]> &colIndex, 
    std::unique_ptr<int[]> &rowPointer, const uint nnz, const uint rows, const uint cols): data(std::move(data)), 
        colIndex(std::move(colIndex)), rowPointer(std::move(rowPointer)), nnz_(nnz), rows_(rows), cols_(cols) { }

template <typename T> CSRMatrix<T>::CSRMatrix(const CSRMatrix<T>& csrMatrix): 
    nnz_(csrMatrix.nnz()), rows_(csrMatrix.rows()), cols_(csrMatrix.cols()) { 
    data = std::make_unique<T[]>(csrMatrix.nnz());
    colIndex = std::make_unique<int[]>(csrMatrix.nnz());
    rowPointer = std::make_unique<int[]>(csrMatrix.rows()+1);
    std::copy(csrMatrix.data.get(), csrMatrix.data.get()+csrMatrix.nnz(), data.get());
    std::copy(csrMatrix.colIndex.get(), csrMatrix.colIndex.get()+csrMatrix.nnz(), colIndex.get());
    std::copy(csrMatrix.rowPointer.get(), csrMatrix.rowPointer.get()+csrMatrix.rows()+1, rowPointer.get());
}

template <typename T> CSRMatrix<T>::~CSRMatrix<T>() { }

template <typename T> const uint CSRMatrix<T>::rows() const { return rows_;}
template <typename T> const uint CSRMatrix<T>::nnz() const { return nnz_;} 
template <typename T> const uint CSRMatrix<T>::cols() const { return cols_;} 

template <typename T> void CSRMatrix<T>::setData(const uint index, const T& value) { data[index] = value; }
template <typename T> const T& CSRMatrix<T>::getData(const uint index) const { return data[index];}
template <typename T> void CSRMatrix<T>::setColIndex(const uint index, const int value) { colIndex[index] = value;}
template <typename T> const int CSRMatrix<T>::getColIndex(const uint index) const { return colIndex[index];}
template <typename T> void CSRMatrix<T>::setRowPointer(const uint index, const int value) { rowPointer[index] = value; }
template <typename T> const int CSRMatrix<T>::getRowPointer(const uint index) const { return rowPointer[index];}

template <typename T> void CSRMatrix<T>:: clear() { 
    std::fill(rowPointer.get(), rowPointer.get()+rows_+1, 0); 
    std::fill(colIndex.get(), colIndex.get()+nnz_, 0);
} 