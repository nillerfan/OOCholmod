//
//  DenseVector.cpp
//  OOCholmod
//  Created by Morten Nobel-Jørgensen on 1/21/13.
//  Copyright (c) 2013 DTU Compute. All rights reserved.
//  License: LGPL 3.0

#include "dense_vector.h"
#include <cassert>
#include <vecLib/cblas.h>
#include "config_singleton.h"

namespace oocholmod {
    
    // bad coffee odd food
#define MAGIC_NUMBER (unsigned long)0xBADC0FFEE0DDF00DL
    
    DenseVector::DenseVector(unsigned int size)
    :size(size)
#ifdef DEBUG
    ,magicNumber(MAGIC_NUMBER)
#endif
    {
        x = cholmod_allocate_dense(size, 1, size /* leading dimension (equal rows) */ , CHOLMOD_REAL, ConfigSingleton::getCommonPtr());
    }
    
    DenseVector::DenseVector(cholmod_dense *x, unsigned int size)
    :x(x), size(size)
#ifdef DEBUG
    ,magicNumber(MAGIC_NUMBER)
#endif
    {
    }
    
    DenseVector::DenseVector(DenseVector&& move)
    :x(move.x), size(move.size)
#ifdef DEBUG
    ,magicNumber(move.magicNumber)
#endif
    {
        move.x = nullptr;
        move.size = 0;
#ifdef DEBUG
        magicNumber = 0;
#endif
    }
    
    DenseVector& DenseVector::operator=(DenseVector&& other){
        if (this != &other){
            if (x != nullptr){
                cholmod_free_dense(&x, ConfigSingleton::getCommonPtr());
            }
            x = other.x;
            size = other.size;
#ifdef DEBUG
            magicNumber = other.magicNumber;
#endif
            
            other.x = nullptr;
            other.size = 0;
#ifdef DEBUG
            other.magicNumber = 0;
#endif
            
        }
        return *this;
    }
    
    DenseVector::~DenseVector(){
        if (x != nullptr){
#ifdef DEBUG
            assert(magicNumber == MAGIC_NUMBER);
            magicNumber = 0;
#endif
            cholmod_free_dense(&x, ConfigSingleton::getCommonPtr());
            x = NULL;
        }
    }
    
    void DenseVector::zero(){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(x != NULL);
#endif
        memset(x->x, 0, size * sizeof(double));
    }
    
    double DenseVector::dot(DenseVector *b){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        return cblas_ddot(getSize(), getData(), 1, b->getData(), 1);
    }
    
    double DenseVector::dot(DenseVector& b){
        return dot(&b);
    }
    
    void DenseVector::fill(double value){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        double *data = getData();
        for (int i=0;i<size;i++){
            data[i] = value;
        }
    }
    
    double DenseVector::length(){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        return cblas_dnrm2(getSize(), getData(), 1);
    }
    
    void DenseVector::scale(double alpha){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        cblas_dscal (getSize(), alpha, getData(), 1);
    }
    
    void DenseVector::divideBy(DenseVector *b){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        assert(b->getSize() >= getSize());
        double *thisData = getData();
        double *bData = b->getData();
        for (int i=0;i<getSize();i++){
            thisData[i] /= bData[i];
        }
    }
    
    void DenseVector::divideBy(DenseVector& b){
        divideBy(&b);
    }
    
    void DenseVector::multiplyWith(DenseVector *b){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(b->getSize() >= getSize());
#endif
        double *thisData = getData();
        double *bData = b->getData();
        for (int i=0;i<getSize();i++){
            thisData[i] *= bData[i];
        }
    }
    
    void DenseVector::multiplyWith(DenseVector& b){
        multiplyWith(&b);
    }
    
    void DenseVector::copyTo(DenseVector *dest){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(dest->getSize() >= getSize());
#endif
        const double *srcPtr = getData();
        double *destPtr = dest->getData();
        memcpy(destPtr, srcPtr, sizeof(double) * getSize());
    }
    
    void DenseVector::copyTo(DenseVector& dest){
        copyTo(&dest);
    }
    
    
    void DenseVector::set(float *inData){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(x != NULL);
#endif
        double *data = getData();
        for (int i=0;i<size;i++){
            data[i] = inData[i];
        }
    }
    
    void DenseVector::set(double *data){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(x != NULL);
#endif
        memcpy(x->x, data, sizeof(double)*size);
    }
    
    void DenseVector::get(double *outData){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        assert(x != NULL);
#endif
        memcpy(outData, x->x, sizeof(double)*size);
    }
    
    void DenseVector::get(float *outData){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        double *data = getData();
        for (int i=0;i<size;i++){
            outData[i] = (float)data[i];
        }
    }
    
    void DenseVector::print(const char* name){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
#endif
        cholmod_print_dense(x, name, ConfigSingleton::getCommonPtr());
        int n_rows = (int)x->nrow;
        int n_cols = (int)x->ncol;
        for (int r = 0; r  < n_rows; r++)
        {
            for (int c = 0; c  < n_cols; c++)
            {
                std::cout << ((double*)x->x)[c*n_rows + r] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
}
