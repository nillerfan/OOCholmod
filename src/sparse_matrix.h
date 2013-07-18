//
//  sparse_matrix.h
//  OOCholmod
//
//  Created by Morten Nobel-Jørgensen / Asger Nyman Christiansen
//  Copyright (c) 2013 DTU Compute. All rights reserved.
//  License: LGPL 3.0

#pragma once

#include <map>
#include <cholmod.h>

#include "config_singleton.h"

namespace oocholmod {
    
    // forward declaration
    class DenseMatrix;
    class Factor;
    
    enum Symmetry {
        SYMMETRIC_LOWER = -1, // Lower triangular part stored
        ASYMMETRIC = 0,
        SYMMETRIC_UPPER = 1, // Upper triangular part stored
    };
    
    enum MatrixState {
        UNINITIALIZED,
        INIT,
        BUILT,
        DESTROYED
    };
    
    /// The sparse matrix must be used in the following way:
    /// 1. Fill the matrix elements using the (unsigned int row, unsigned int column) function operator
    /// 2. Call build()
    /// 3. Update matrix with elements using the (unsigned int row, unsigned int column) function operator
    ///
    class SparseMatrix {
        friend class Factor;
    public:
        /// nrow # of rows of A
        /// ncol # of columns of A
        /// initialNumberOfElements. If exceeded (during initialization of the matrix) the number of elements will automatically grow with a factor of 1.5
        SparseMatrix(unsigned int nrow = 0, unsigned int ncol = 1, bool symmetric = false, int initialNumberOfElements = 200);
        SparseMatrix(cholmod_sparse *sparse);
        SparseMatrix(SparseMatrix&& move);
        SparseMatrix& operator=(SparseMatrix&& other);
        
        virtual ~SparseMatrix();
        
        MatrixState getMatrixState() const;
        
        // Addition
        friend SparseMatrix operator+(const SparseMatrix& LHS, const SparseMatrix& RHS);
        friend SparseMatrix&& operator+(SparseMatrix&& LHS, const SparseMatrix& RHS);
        friend SparseMatrix&& operator+(const SparseMatrix& LHS, SparseMatrix&& RHS);
        friend SparseMatrix&& operator+(SparseMatrix&& LHS, SparseMatrix&& RHS);
        
        // Multiplication
        friend SparseMatrix operator*(const SparseMatrix& LHS, const double& RHS);
        friend SparseMatrix&& operator*(SparseMatrix&& LHS, const double& RHS);
        friend SparseMatrix operator*(const double& LHS, const SparseMatrix& RHS);
        friend SparseMatrix&& operator*(const double& LHS, SparseMatrix&& RHS);
        
        friend SparseMatrix operator*(const SparseMatrix& LHS, const SparseMatrix& RHS);
        
        friend DenseMatrix operator*(const DenseMatrix& LHS, const SparseMatrix& RHS);
        friend DenseMatrix operator*(const SparseMatrix& LHS, const DenseMatrix& RHS);
        
        // Transpose
        void transpose();
        friend SparseMatrix transposed(const SparseMatrix& M);
        friend SparseMatrix&& transposed(SparseMatrix&& M);
        
        // Solve
        friend DenseMatrix solve(const SparseMatrix& A, const DenseMatrix& b);
        friend SparseMatrix solve(const SparseMatrix& A, const SparseMatrix& b);
        friend SparseMatrix solve(const Factor& F, const SparseMatrix& b);
        
        void build();
        
        Factor analyze() const;
        
        void zero();
        
        void setSymmetry(Symmetry symmetry);
        Symmetry getSymmetry() const { return symmetry; }
        
        int getRows() const { return nrow; }
        
        int getColumns() const { return ncol; }
        
        /// Print debugging information
        void print(const char* name = "") const;
        
        void swap(SparseMatrix& other);
        
        inline double operator()(unsigned int row, unsigned int column = 0) const
        {
            return getValue(row, column);
        }
        
        inline double& operator()(unsigned int row, unsigned int column = 0)
        {
            if (sparse != nullptr){
                return getValue(row, column);
            } else {
                return initAddValue(row, column);
            }
        }
        
        bool operator==(const SparseMatrix& RHS);
        
    private:
        SparseMatrix(const SparseMatrix& that) = delete; // prevent copy constructor
        SparseMatrix operator=(const SparseMatrix& other) = delete; // prevent copy assignment operator
        void buildLookupIndexFromSparse();
        inline long key(unsigned int row, unsigned int column) const{
            int shiftBits = sizeof(long)*8/2; // shift half of the bits of a long
            return (((long)row)<<shiftBits)+column;
        }
        void assertValidIndex(unsigned int row, unsigned int column) const;
        void assertHasSparse() const;
        void increaseTripletCapacity();
        void assertValidInitAddValue(unsigned int row, unsigned int column) const;
        
        inline int getIndex(unsigned int row, unsigned int column) const
        {
            assertValidIndex(row, column);
            if (symmetry == SYMMETRIC_UPPER && row > column) {
                std::swap(row, column);
            } else if (symmetry == SYMMETRIC_LOWER && row < column) {
                std::swap(row, column);
            }
            auto iter = lookupIndex.find(key(row,column));
            if (iter == lookupIndex.end()){
                return -1;
            }
            return iter->second;
        }
        
        inline double& initAddValue(unsigned int row, unsigned int column)
        {
            if (!triplet){
                triplet = cholmod_allocate_triplet(nrow, ncol, maxTripletElements, symmetry, CHOLMOD_REAL, ConfigSingleton::getCommonPtr());
                values = (double *)triplet->x;
                iRow = (int *)triplet->i;
                jColumn = (int *)triplet->j;
            } else if (triplet->nnz == maxTripletElements ){
                increaseTripletCapacity();
            }
            assertValidInitAddValue(row, column);
            iRow[triplet->nnz] = row;
            jColumn[triplet->nnz] = column;
            values[triplet->nnz] = 0;
            
            triplet->nnz++;
            return values[triplet->nnz - 1];
        }
        
        inline double& getValue(unsigned int row, unsigned int column)
        {
            assertHasSparse();
            if (lookupIndex.empty()){
                buildLookupIndexFromSparse();
            }
            int index = getIndex(row, column);
            if (index == -1){
                static double zero = 0;
                zero = 0;
                return zero;
            }
            return ((double*)sparse->x)[index];
        }
        
        double getValue(unsigned int row, unsigned int column) const
        {
            assertHasSparse();
            if (lookupIndex.empty()){
                SparseMatrix & nonConst = const_cast<SparseMatrix &>(*this);
                nonConst.buildLookupIndexFromSparse();
            }
            int index = getIndex(row, column);
            if (index == -1){
                return 0;
            }
            return ((double*)sparse->x)[index];
        }
        
        cholmod_sparse *sparse;
        cholmod_triplet *triplet;
        unsigned int nrow;
        unsigned int ncol;
        double *values;
        std::map<unsigned long, unsigned int> lookupIndex;
        int *iRow;
        int *jColumn;
        Symmetry symmetry;
        int maxTripletElements;
    };
    
    // Addition
    SparseMatrix operator+(const SparseMatrix& LHS, const SparseMatrix& RHS);
    SparseMatrix&& operator+(SparseMatrix&& LHS, const SparseMatrix& RHS);
    SparseMatrix&& operator+(const SparseMatrix& LHS, SparseMatrix&& RHS);
    SparseMatrix&& operator+(SparseMatrix&& LHS, SparseMatrix&& RHS);
    
    // Multiplication
    SparseMatrix operator*(const SparseMatrix& LHS, const double& RHS);
    SparseMatrix&& operator*(SparseMatrix&& LHS, const double& RHS);
    SparseMatrix operator*(const double& LHS, const SparseMatrix& RHS);
    SparseMatrix&& operator*(const double& LHS, SparseMatrix&& RHS);
    
    SparseMatrix operator*(const SparseMatrix& LHS, const SparseMatrix& RHS);
    
    // DenseMatrix times SparseMatrix (Note that SparseMatrix times DenseMatrix may be faster).
    DenseMatrix operator*(const DenseMatrix& LHS, const SparseMatrix& RHS);
    DenseMatrix operator*(const SparseMatrix& LHS, const DenseMatrix& RHS);
    
    // Transpose
    SparseMatrix transposed(const SparseMatrix& M);
    SparseMatrix&& transposed(SparseMatrix&& M);
    
    // Swap
    void swap(SparseMatrix& v1, SparseMatrix& v2);
    
    // Solve
    DenseMatrix solve(const SparseMatrix& A, const DenseMatrix& b);
    SparseMatrix solve(const SparseMatrix& A, const SparseMatrix& b);
}

