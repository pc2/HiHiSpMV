#pragma once

#include "includes.hpp"

template<typename T>
class DenseVector {
    private:
        int n;

    public:
        std::unique_ptr<T[]> elements;

        DenseVector<T>();
        DenseVector<T>(int n);
        DenseVector<T>(int n, const T &value);
        DenseVector<T>(std::unique_ptr<T[]> elements, int n);

        DenseVector(const DenseVector& vector);
        DenseVector& operator=(const DenseVector& other);

        ~DenseVector<T>();

        int size() const;
        
        T& operator[](int index);
        const T& operator[](const int index) const;

        void set(int index, const T value);
        void setAll(const T value);
        const T& at(int index) const;
};

template <typename T> DenseVector<T>::DenseVector(): DenseVector(0) { }
template <typename T> DenseVector<T>::DenseVector(int n): n(n), elements(std::make_unique<T[]>(n)) { }
template <typename T> DenseVector<T>::DenseVector(int n, const T &value): n(n), elements(std::make_unique<T[]>(n)) { 
    std::fill(elements.get(), elements.get()+n, value);
}

template <typename T> DenseVector<T>::DenseVector(std::unique_ptr<T[]> elements, int n): n(n), elements(std::move(elements)) { }
template <typename T> DenseVector<T>::DenseVector(const DenseVector<T>& other) : n(other.n), elements(std::make_unique<T[]>(n)) { 
    std::copy(other.elements.get(), other.elements.get()+n, elements.get());
}

template <typename T> DenseVector<T>& DenseVector<T>::operator=(const DenseVector<T>& other) { 
    n = other.n;
    elements = (std::make_unique<T[]>(n)); // Shallow copy
    std::copy(other.elements.get(), other.elements.get()+n, elements.get());
    return *this;
}

template <typename T> DenseVector<T>::~DenseVector() { }

template <typename T> int DenseVector<T>::size() const { return n;}
template <typename T> T& DenseVector<T>::operator[](int index) { return elements[index]; }
template <typename T> const T& DenseVector<T>::operator[](const int index) const { return elements[index]; }
template <typename T> void DenseVector<T>::set(int index, const T value) { elements[index] = value; }
template <typename T> void DenseVector<T>::setAll(const T value) { 
    std::fill(elements.get(), elements.get()+n, value);
}
template <typename T> const T& DenseVector<T>::at(int index) const { return elements[index];}