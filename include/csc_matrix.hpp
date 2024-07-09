#pragma once

#include "includes.hpp"

template<typename T>
class CSCMatrix {
    
    private:
        int nnz_, cols_;
    // Todo: change the order of initialization in construtor so -Wall doesn't warning about reordring
    public:
        std::unique_ptr<T[]> data;
        std::unique_ptr<int[]> rowIndex;
        std::unique_ptr<int[]> colPointer;

        CSCMatrix<T>();
        CSCMatrix<T>(const int nnz, const int cols);
        CSCMatrix<T>(std::unique_ptr<T[]> &data, std::unique_ptr<int[]> &rowIndex,
            std::unique_ptr<int[]> &colPointer, const int nnz, const int cols);

        CSCMatrix<T>(const CSCMatrix<T>& CSCMatrix);

        ~CSCMatrix<T>();

        const int cols() const;
        const int nnz() const;

        // T& operator()(const int x, const int y);
        // const T& operator()(const int x, const int y);

        void setData(const int index, const T& value);
        const T& getData(int index) const;

        void setRowIndex(const int index, const int value);
        const int getRowIndex(int index) const;

        void setColPointer(const int index, const int value);
        const int getColPointer(int index) const;

        void clear();
};

template <typename T> CSCMatrix<T>::CSCMatrix(): CSCMatrix(0, 0){ }
template <typename T> CSCMatrix<T>::CSCMatrix(const int nnz, const int cols): 
    data(std::make_unique<T[]>(nnz)), rowIndex(std::make_unique<int[]>(nnz)),
    colPointer(std::make_unique<int[]>(cols+1)), nnz_(nnz), cols_(cols) { }

template <typename T> CSCMatrix<T>::CSCMatrix(std::unique_ptr<T[]> &data, std::unique_ptr<int[]> &rowIndex, 
    std::unique_ptr<int[]> &colPointer, const int nnz, const int cols): data(std::move(data)), 
        rowIndex(std::move(rowIndex)), colPointer(std::move(colPointer)), nnz_(nnz), cols_(cols) { }

template <typename T> CSCMatrix<T>::CSCMatrix(const CSCMatrix<T>& cscMatrix): nnz_(cscMatrix.nnz()), cols_(cscMatrix.cols()) { 
    data = std::make_unique<T[]>(cscMatrix.nnz());
    rowIndex = std::make_unique<int[]>(cscMatrix.nnz());
    colPointer = std::make_unique<int[]>(cscMatrix.cols()+1);
    std::copy(cscMatrix.data.get(), cscMatrix.data.get()+cscMatrix.nnz(), data.get());
    std::copy(cscMatrix.rowIndex.get(), cscMatrix.rowIndex.get()+cscMatrix.nnz(), rowIndex.get());
    std::copy(cscMatrix.colPointer.get(), cscMatrix.colPointer.get()+cscMatrix.cols()+1, colPointer.get());
}

template <typename T> CSCMatrix<T>::~CSCMatrix<T>() { }

template <typename T> const int CSCMatrix<T>::cols() const { return cols_;}
template <typename T> const int CSCMatrix<T>::nnz() const { return nnz_;} 
template <typename T> void CSCMatrix<T>::setData(const int index, const T& value) { data[index] = value; }
template <typename T> const T& CSCMatrix<T>::getData(const int index) const { return data[index];}
template <typename T> void CSCMatrix<T>::setRowIndex(const int index, const int value) { rowIndex[index] = value;}
template <typename T> const int CSCMatrix<T>::getRowIndex(const int index) const { return rowIndex[index];}
template <typename T> void CSCMatrix<T>::setColPointer(const int index, const int value) { colPointer[index] = value; }
template <typename T> const int CSCMatrix<T>::getColPointer(const int index) const { return colPointer[index];}
template <typename T> void CSCMatrix<T>:: clear() { 
    std::fill(rowIndex.get(), rowIndex.get()+nnz_, 0);
    std::fill(colPointer.get(), colPointer.get()+cols_+1, 0);
} 