//
//  SparseMatrix.cpp
//  OOCholmod
//
//  Created by Morten Nobel-Jørgensen on 1/21/13.
//  Copyright (c) 2013 DTU Compute. All rights reserved.
//  License: LGPL 3.0 

#include "sparse_matrix.h"
#include <cassert>

#include "dense_vector.h"
#include "config_singleton.h"

using namespace std;

// bad coffee odd food
#define MAGIC_NUMBER (unsigned long)0xBADC0FFEE0DDF00DL

namespace oocholmod{

SparseMatrix::SparseMatrix(unsigned int nrow, unsigned int ncol, int maxSize)
:sparse(nullptr), triplet(nullptr), nrow(nrow), ncol(ncol)
#ifdef DEBUG
,magicNumber(MAGIC_NUMBER)
#endif
{
    int elements;
    if (maxSize == 0) {
        elements = (nrow*(ncol+1))/2; // triangular number
    } else {
        elements = maxSize;
    }
    triplet = cholmod_allocate_triplet(nrow, ncol, elements, (int)SYMMETRIC_UPPER, CHOLMOD_REAL, ConfigSingleton::getCommonPtr());
    values = (double *)triplet->x;
    iRow = (int *)triplet->i;
	jColumn = (int *)triplet->j;
    this->symmetry = SYMMETRIC_UPPER;
#ifdef DEBUG
    assert(nrow == ncol); // must be square
    maxElements = elements;
#endif
}

SparseMatrix::SparseMatrix(cholmod_sparse *sparse)
:sparse(sparse), triplet(nullptr), nrow((unsigned int)sparse->nrow), ncol((unsigned int)sparse->ncol)
#ifdef DEBUG
,magicNumber(MAGIC_NUMBER)
#endif
{
    buildLookupIndexFromSparse();
}

SparseMatrix::SparseMatrix(SparseMatrix&& move)
:sparse(move.sparse), triplet(move.triplet), nrow(move.nrow), ncol(move.ncol), lookupIndex(std::move(move.lookupIndex)), iRow(move.iRow), jColumn(move.jColumn), symmetry(move.symmetry)
#ifdef DEBUG
    ,magicNumber(move.magicNumber), maxElements(move.maxElements)
#endif
{
    move.sparse = nullptr;
    move.triplet = nullptr;
    move.values = nullptr;
    move.iRow = nullptr;
    move.jColumn = nullptr;
#ifdef DEBUG
    move.magicNumber = 0L;
    move.maxElements = 0;
#endif
}

SparseMatrix& SparseMatrix::operator=(SparseMatrix&& other){
    if (this != &other){
        if (sparse != NULL){
            cholmod_free_sparse(&sparse, ConfigSingleton::getCommonPtr());
        }
        if (triplet != NULL){
            cholmod_free_triplet(&triplet, ConfigSingleton::getCommonPtr());
        }
        
        sparse = other.sparse;
        triplet = other.triplet;
        nrow = other.nrow;
        ncol = other.ncol;
        lookupIndex = std::move(other.lookupIndex);
        iRow = other.iRow;
        jColumn = other.jColumn;
        symmetry = other.symmetry;
#ifdef DEBUG
        magicNumber = other.magicNumber;
        maxElements = other.maxElements;
#endif
    }
    return *this;
}


SparseMatrix::~SparseMatrix(){
    if (sparse != nullptr || triplet != nullptr){
#ifdef DEBUG
        assert(magicNumber == MAGIC_NUMBER);
        magicNumber = 0;
#endif
        if (sparse != NULL){
            cholmod_free_sparse(&sparse, ConfigSingleton::getCommonPtr());
            sparse = NULL;
        }
        if (triplet != NULL){
            cholmod_free_triplet(&triplet, ConfigSingleton::getCommonPtr());
            triplet = NULL;
        }
    }
}

void SparseMatrix::setNullSpace(CholmodDenseVector *N){
#ifdef DEBUG
    assert(sparse != NULL);
    assert(magicNumber == MAGIC_NUMBER);
#endif
    // naive implementation: Todo run fast
    CholmodDenseVector &v = *N;
    int idx = 0;
    for (int j=0;j<ncol;j++){
        int iFrom = ((int*)sparse->p)[j];
        int iTo = ((int*)sparse->p)[j+1]-1;
        for (int i=iFrom;i<=iTo;i++){
            int row = ((int*)sparse->i)[i];
            ((double*)sparse->x)[idx] *= v[row]*v[j];
            idx++;
        }
    }
    for (int i=0;i<v.getSize();i++){
        if (v[i] == 0){
            this->setValue(i,i,1);
        }
    }
}

void SparseMatrix::setNullSpace(CholmodDenseVector& N){
    setNullSpace(&N);
}

// computes this * X and store the result in res
void SparseMatrix::multiply(CholmodDenseVector *X, CholmodDenseVector *res, double alpha, double beta){
#ifdef DEBUG
    assert(magicNumber == MAGIC_NUMBER);
    assert(res != NULL);
    assert(X->getSize() == res->getSize());
    assert(X != res);
    assert(nrow == X->getSize());
#endif    
    
    // alpha*(A*X) + beta*Y
    double _alpha[2] = {alpha,alpha};
    double _beta[2] = {beta,beta};
    // int cholmod_sdmult(cholmod_sparse *A, ￼￼int transpose, double alpha [2], double beta [2], cholmod_dense *X, cholmod_dense *Y, cholmod_common *Common );
    cholmod_sdmult(sparse, false, _alpha, _beta, X->getHandle(), res->getHandle(), ConfigSingleton::getCommonPtr());
}

CholmodDenseVector SparseMatrix::multiply(CholmodDenseVector& X, double alpha, double beta){
    CholmodDenseVector res(X.getSize());
    multiply(X, res, alpha, beta);
    return res;
}

void SparseMatrix::multiply(CholmodDenseVector& X, CholmodDenseVector& res, double alpha, double beta){
    multiply(&X, &res, alpha, beta);
}

void SparseMatrix::build(bool readOnly){
#ifdef DEBUG
    assert(sparse == NULL);
    assert(magicNumber == MAGIC_NUMBER);
#endif
    sparse = cholmod_triplet_to_sparse(triplet, triplet->nnz, ConfigSingleton::getCommonPtr());
    cholmod_free_triplet(&triplet, ConfigSingleton::getCommonPtr());
    triplet = NULL;
    values = NULL;
    iRow = NULL;
	jColumn = NULL;
    
    // build lookup index
#ifdef DEBUG
    assert(sparse->stype == symmetry);
    assert(sparse->packed);
#endif
    if (!readOnly){
        buildLookupIndexFromSparse();
    }
}

void SparseMatrix::buildLookupIndexFromSparse(){
#ifdef DEBUG
    assert(magicNumber == MAGIC_NUMBER);
#endif
    lookupIndex.clear();
    // In packed form, the nonzero pattern of column j is in A->i [A->p [j] ... A->p [j+1]-1]
    int idx = 0;
    for (int j=0;j<ncol;j++){
        int iFrom = ((int*)sparse->p)[j];
        int iTo = ((int*)sparse->p)[j+1]-1;
        for (int i=iFrom;i<=iTo;i++){
            int row = ((int*)sparse->i)[i];
            lookupIndex[key(row, j)] = idx;
            idx++;
        }
    }
}

CholmodFactor *SparseMatrix::analyzePtr(){
#ifdef DEBUG
    assert(sparse != NULL);
    assert(magicNumber == MAGIC_NUMBER);
#endif
    cholmod_factor *L = cholmod_analyze(sparse, ConfigSingleton::getCommonPtr());
    return new CholmodFactor(L);
}

CholmodFactor SparseMatrix::analyze(){
#ifdef DEBUG
    assert(sparse != NULL);
    assert(magicNumber == MAGIC_NUMBER);
#endif
    cholmod_factor *L = cholmod_analyze(sparse, ConfigSingleton::getCommonPtr());
    return CholmodFactor(L);
}


void SparseMatrix::zero(){
#ifdef DEBUG
    assert(sparse != NULL);
    assert(magicNumber == MAGIC_NUMBER);
#endif
    memset(sparse->x, 0, sparse->nzmax * sizeof(double));
}

void SparseMatrix::print(const char* name){
#ifdef DEBUG
    assert(magicNumber == MAGIC_NUMBER);
#endif
    if (sparse){
        cholmod_print_sparse(sparse, name, ConfigSingleton::getCommonPtr());
        cholmod_dense *dense = cholmod_sparse_to_dense(sparse, ConfigSingleton::getCommonPtr());
        int n_rows = (int)dense->nrow;
        int n_cols = (int)dense->ncol;
        for (int r = 0; r  < n_rows; r++)
        {
            for (int c = 0; c  < n_cols; c++)
            {
                cout << ((double*)dense->x)[c*n_rows + r] << " ";
            }
            cout << endl;
        }
        cholmod_free_dense(&dense, ConfigSingleton::getCommonPtr());
        cout << endl;
        cout << "Packed "<<sparse->packed<< endl;
        cout << "p: ";
        for (int i=0;i<=sparse->ncol;i++){
            printf("%4i ", ((int*)sparse->p)[i]);
        }
        cout << endl;
        cout << "i: ";
        for (int i=0;i<sparse->nzmax;i++){
            printf("%4i ", ((int*)sparse->i)[i]);
        }
        cout << endl;
        cout << "x: ";
        for (int i=0;i<sparse->nzmax;i++){
            printf("%3.3f ", ((double*)sparse->x)[i]);
        }
        cholmod_free_dense(&dense, ConfigSingleton::getCommonPtr());
        cout << endl;
    }
}

void SparseMatrix::assertValidIndex(unsigned int row, unsigned int column){
#ifdef DEBUG
    assert(symmetry == SYMMETRIC_UPPER);
    assert(row < nrow);
    assert(column < ncol);
    if (symmetry == SYMMETRIC_UPPER) {
        assert(row <= column);
    } else if (symmetry == SYMMETRIC_LOWER) {
        assert(row >= column);
    }
    
#endif
}

void SparseMatrix::assertHasSparse(){
#ifdef DEBUG
    assert(sparse != NULL); // matrix must be build
#endif
}

void SparseMatrix::assertValidInitAddValue(unsigned int row, unsigned int column, double value){
#ifdef DEBUG
    assert(sparse == NULL); // must be called before matrix build
    assert(triplet->nnz < maxElements);
    assert(row < nrow);
    assert(column < ncol);
    if (symmetry == SYMMETRIC_UPPER) {
        assert(row <= column);
    } else if (symmetry == SYMMETRIC_LOWER) {
        assert(row >= column);
    }
    int shiftBits = sizeof(long)*8/2; // shift half of the bits of a long
    long maxId = (long)pow(2, shiftBits);
    assert (row < maxId);
    assert (column < maxId);
#endif
}
    
    
    SparseMatrix operator+(const SparseMatrix& LHS, const SparseMatrix& RHS)
    {
        assert(LHS.nrow == RHS.nrow && LHS.ncol == RHS.ncol);
        double scale[] = {1.,1.};
        cholmod_sparse *sparse = cholmod_add(LHS.sparse, RHS.sparse, scale, scale, true, true, ConfigSingleton::getCommonPtr());
        return SparseMatrix(sparse);
    }
    
    SparseMatrix&& operator+(SparseMatrix&& LHS, const SparseMatrix& RHS)
    {
        assert(LHS.sparse && RHS.sparse);
        assert(LHS.nrow == RHS.nrow && LHS.ncol == RHS.ncol);
        double scale[] = {1.,1.};
        cholmod_sparse *sparse = cholmod_add(LHS.sparse, RHS.sparse, scale, scale, true, true, ConfigSingleton::getCommonPtr());
        cholmod_free_sparse(&LHS.sparse, ConfigSingleton::getCommonPtr());
        LHS.sparse = sparse;
        return std::move(LHS);
    }
    
    SparseMatrix&& operator+(const SparseMatrix& LHS, SparseMatrix&& RHS)
    {
        return std::move(RHS) + LHS;
    }
    
    SparseMatrix&& operator+(SparseMatrix&& LHS, SparseMatrix&& RHS)
    {
        return std::move(LHS) + RHS;
    }
    
}