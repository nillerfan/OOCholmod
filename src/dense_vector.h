//
//  dense_vector.h
//  OOCholmod
//
//  Created by Morten Nobel-Jørgensen on 1/21/13.
//  Copyright (c) 2013 DTU Compute. All rights reserved.
//  License: LGPL 3.0 

#pragma once

#include <iostream>
#include <cassert>


#include <cholmod.h>

namespace oocholmod {
    
class CholmodDenseVector {
public:
    CholmodDenseVector(unsigned int size);
    CholmodDenseVector(cholmod_dense *x, unsigned int size);
    CholmodDenseVector(CholmodDenseVector&& move);
    CholmodDenseVector& operator=(CholmodDenseVector&& other);
    virtual ~CholmodDenseVector();
    inline double *getData(){ return (double *)(x->x); };
    inline double *getData() const { return (double *)(x->x); };
    inline int getSize() { return size; }
    void copyTo(CholmodDenseVector *dest);
    void copyTo(CholmodDenseVector& dest);
    void zero();
    // computes the L^2 norm of the vector
    double length();
    void scale(double alpha);
    // elementwise division
    void divideBy(CholmodDenseVector *b);
    void divideBy(CholmodDenseVector& b);
    // elementwise multiplication
    void multiplyWith(CholmodDenseVector *b);
    void multiplyWith(CholmodDenseVector& b);
    double dot(CholmodDenseVector *b);
    double dot(CholmodDenseVector& b);
    void fill(double value);
    void set(float *data);
    void set(double *data);
    void get(double *outData);
    void get(float *outData);
    inline void add(unsigned int index, double value) {
#ifdef DEBUG
        assert(index < size);
#endif
        getData()[index] += value;
    };
    
    inline double operator [](int i) const    {return getData()[i];}
    inline double & operator [](int i) {return getData()[i];}
    inline cholmod_dense *getHandle() { return x; }
    void print(const char* name);
private:
    CholmodDenseVector(const CholmodDenseVector& that) = delete; // prevent copy constructor
    cholmod_dense *x;
    unsigned int size;
#ifdef DEBUG
    unsigned long magicNumber;
#endif
};
    
}
