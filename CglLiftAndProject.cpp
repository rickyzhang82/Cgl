// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// WARNING: work-in-progress. Not functional.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cassert>
#include <cfloat>
#include <iostream>

#include "CoinHelperFunctions.hpp"
#include "CglLiftAndProject.hpp"
#include "OsiPackedVector.hpp"
#include "CoinSort.hpp"
#include "OsiPackedMatrix.hpp"

//-----------------------------------------------------------------------------
// Generate knapsack cover cuts
//------------------------------------------------------------------- 
void CglLiftAndProject::generateCuts(const OsiSolverInterface & si, 
						OsiCuts & cs ) const
{
  // Assumes the mixed 0-1 problem 
  //
  //   min {cx: <Atilde,x> >= btilde} 
  //
  // is in cannonical form with all bounds,
  // including x_t>=0, -x_t>=-1 for x_t binary,
  // explicitly stated in the constraint matrix. 
  // See ~/COIN/Examples/Cgl2/cgl2.cpp 
  // for a general purpose "convert" function. 

  // Reference [BCC]: Balas, Ceria, and Corneujols,
  // "A lift-and-project cutting plane algorithm
  // for mixed 0-1 program", Math Prog 58, (1993) 
  // 295-324.

  // This implementation uses Normalization 1.

  // Given cannonical problem and
  // the lp-relaxation solution, x,
  // the LAP cut generator attempts to construct
  // a cut for every x_j such that 0<x_j<1
  // [BCC:307]
 

  // x_j is the strictly fractional binary variable
  // the cut is generated from
  int j = 0; 
  int jSpot1, jSpot2;// position of x_j in BIndices 

  // Get basic problem information
  // let Atilde be an m by n matrix
  const int m = si.getNumRows(); 
  const int n = si.getNumCols(); 
  const double * x = si.getColSolution();

  // Remember - Atildes may have gaps..
  const OsiPackedMatrix * Atilde = si.getMatrixByRow();
  const double * AtildeElements =  Atilde->getElements();
  const int * AtildeIndices =  Atilde->getIndices();
  const int * AtildeStarts = Atilde->getVectorStarts();
  const int * AtildeLengths = Atilde->getVectorLengths();  
  const int AtildeFullSize = AtildeStarts[m+1];
  const double * btilde = si.getRowLower();

  // Set up memory for system (10) [BCC:307]
  // (the problem over the polar cone)
  // 
  // min <<x^T,Atilde^T>,u> + x_ju_0
  // s.t.
  //     <B,w> = 0
  //        w   >= 0
  // where 
  // w = (u,v,beta,v_0,u_0)in BCC notation and
  //  
  // B = Atilde^T  -Atilde^T  e_0 -e_j e_j
  //     btilde^T   e_0^T     -1   0   0
  //     e_0^T      btilde^T  -1   1   0

  // ^T indicates Transpose
  // e_0 is a (AtildeNCols x 1) vector of all zeros 
  // e_j is e_0 with a 1 in the jth position

  // Storing B in column order. B is a (n+2 x 2m+3) matrix 
  // But need to allow for possible gaps in Atilde.
  // At each iteration, only need to change 2 cols and objfunc
  // Sane design of OsiSolverInterface does not permit mucking
  // with matrix.
  // Becuase we must delete and add cols to alter matrix,
  // and we can only add columns on the end of the matrix
  // put the v_0 and u_0 columns on the end.
  // rather than as described in [BCC]
 
  // Initially allocating B with space for v_0 and u_O cols
  // but not populating.

  int twoM = 2*m;
  int BNumRows = n+2;
  int BNumCols = twoM+3;
  int BFullSize = 2*AtildeFullSize+twoM+5;
  double * BElements = new double[BFullSize];
  int * BIndices = new int[BFullSize];
  int * BStarts = new int[BNumCols+1];
  int * BLengths = new int[BNumCols];


  int i, ij, k=0;
  int nPlus1=n+1;
  int nPlus2=n+2;
  int offset = AtildeStarts[m]+m;
  for (i=0; i<m; i++){
    for (ij=AtildeStarts[i];ij<AtildeStarts[i]+AtildeLengths[i];ij++){
      BElements[k]=AtildeElements[ij];
      BElements[k+offset]=-AtildeElements[ij];
      BIndices[k]= AtildeIndices[ij];
      BIndices[k+offset]= AtildeIndices[ij];
	
      k++;
    }
    BElements[k]=btilde[i];
    BElements[k+offset]=btilde[i];
    BIndices[k]=nPlus1;
    BIndices[k+offset]=nPlus2;
    BStarts[i]= AtildeStarts[i]+i;
    BStarts[i+m]=offset+BStarts[i];// = AtildeStarts[m]+m+AtildeStarts[i]+i
    BLengths[i]= AtildeLengths[i]+1;
    BLengths[i+m]= AtildeLengths[i]+1;
    k++;
  }
  i=twoM;

  ////// Commented out b/c rearranging u_0 and v_0 vectors.
  // Store column corresponding to u_0
  // BElements[k]=1;
  // BIndices[k]=j;
  // k++;
  // BStarts[i]=BStarts[i-1]+AtildeLengths[m-1];
  // BLengths[i]= 1;
  // i++;

  // Store column corresponding to v_0
  // BElements[k]=-1;
  // BIndices[k]=j;
  // k++;
  // BElements[k]=1;
  // BIndices[k]=nPlus2;
  // k++
  // BStarts[i]=BStarts[i-1]+1;
  // BLengths[i]= 2;
  // i++;

  // Store column coresponding to beta
  BElements[k]=-1;
  BIndices[k]=nPlus1;
  k++;
  BElements[k]=-1;
  BIndices[k]=nPlus2;
  k++;
  BStarts[i]=BStarts[i-1]+AtildeLengths[m-1]; // only line that change w/rearr.
  BLengths[i]= 2;
  i++;

  // Mark end of BStarts
  BStarts[i]=BStarts[i-1]+2;

  // Set lower bound on u and v
  // u_0, v_0 >= 0 is implied but this is easier
  const double INFINITY = si.getInfinity();
  double * BColLowers = new double[BNumCols];
  double * BColUppers = new double[BNumCols];
  CoinFillN(BColLowers,BNumCols,0.0);  
  CoinFillN(BColUppers,BNumCols,INFINITY);  

  // Set row lowers and uppers to zero
  double * BRowLowers = new double[BNumRows];
  double * BRowUppers = new double[BNumRows];
  CoinFillN(BRowLowers,BNumCols,0.0);  
  CoinFillN(BRowUppers,BNumCols,0.0);  

  // Calculate base objective <<x^T,Atilde^T>,u>
  // at each iteration coefficient u_0
  // changes to <x^T,e_j>
  double * BObjective= new double[BNumCols];
  CoinFillN(BObjective,BNumCols,0.0);  

  // Number of cols and size of Elements vector
  // in B without v_0 and u_0 cols
  int BNumColsLessTwo = BNumCols-2;
  int BFullSizeLessThree = BFullSize-3;

  // Load B matrix into a column orders OsiPackedMatrix
  // Need to change to values in BIndices each iterations - ask LL.
  OsiPackedMatrix * BMatrix = new OsiPackedMatrix(true, BNumRows,
						  BNumColsLessTwo, 
						  BFullSizeLessThree,
						  BElements,BIndices, 
						  BStarts,BLengths);
  // Load problem into a solver interface 
  // ? Maybe better to assign problem rather than load ?
  OsiSolverInterface * coneSi = si.clone(false);
  coneSi->loadProblem (*BMatrix, BColLowers, BColUppers, 
		      BObjective,
		      BRowLowers, BRowUppers);

  // Problem sense should defalut to "min" by default, but ...
  coneSi->setObjSense(1.0);


  // B set up with out v_0 and u_0 columns
  //   but with empty memory allocated.
  // Calculate base objective
  // bounded = false;
  // int j=0;
  // while (!bounded){
  //   bool found = findFractionalBinary(si,x,&j,n);
  //                                  // starts looking at position j
  //                                  // and increments over all variables
  //                                  // returns new j & true, 
  //                                  // else returns n & false
  //   if (! found), return {clean up memory}
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   set objective[u_0]=x_j;
  //   solve min{objw:Bw=0, w>=0}
  //   if unbounded, continue;
  //   bounded = true;
  //   get warmstart;
  //   caculate and add cuts;
  //   reset objective[u_0] // maynot need (?)
  //   delete col for u_0   // what hapens to obj when col deleted?
  //   delete col for v_0
  // }
  // while(true) {
  //   bool found = findFractionalBinary(si,x,&j,n);
  //   if (! found), return {clean up memory} 
  //   (IMPROVEME:use warmstart info to see if j attractive) 
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   set objective[u_0]=x_j;
  //   set warmstart information
  //   solve min{objw:Bw=0, w>=0}
  //   if (bounded)unbounded
  //     get warmstart information
  //     caculate and add cuts;
  //   reset objective[u_0] // may not need
  //   delete col for u_0   // what happen to obj when col deleted?
  //   delete col for v_0
  // }
  // clean up memory
  // return 0;

  // B without u_0 and v_0, assign problem
  // Calculate base objective <<x^T,Atilde^T>,u>
  // bool haveWarmStart = false;
  // For (j=0; j<n, j++)
  //   if (!isBinary(x_j) || x_j<=0 || x_j>=1) continue
  //   // IMPROVEME: if(haveWarmStart) check if j attractive
  //   add {-e_j,0,-1} col for v_0
  //   add {e_j,0,0} col for u_0
  //   objective[u_0] = u_0x_j 
  //   if (haveWarmStart) 
  //      set warmstart info
  //   solve min{objw:Bw=0, w>=0}
  //   if (bounded)
  //      get warmstart info
  //      haveWarmStart=true;
  //      ustar = optimal u solution
  //      ustar_0 = optimal u_o solution
  //      alpha^T= <ustar^T,Atilde> -ustar_0e_j^T
  //      (double check <alpha^T,x> >= beta_ should be violated)
  //      add <alpha^T,x> >= beta_ to cutset 
  //   endif
  //   reset objective[u_0] // may not need
  //   delete col for u_0   // what happen to obj when col deleted?
  //   delete col for v_0
  // endFor
  // clean up memory
  // return 0;

  bool equalObj1, equalObj2;
  OsiRelFltEq eq;

  for (j=0;j<n;j++){
    if (!si.isBinary(j)) continue; // better to ask coneSi?
    equalObj1=eq(x[j],0);
    equalObj2=eq(x[j],1);
    if (equalObj1 || equalObj2) continue;

    // Would really be nice if I could set 
    // all objective coefficients in one shot...
    // 
    // calculate <<x^T,Atilde^T>,u>  = <Atilde, x
    // add u_0x_j    
    //    coneSi->setObjCoeff( int elementIndex, double elementValue );


    // cant do because don't know how the solver interface repres the matrix
    // const_cast<int*> (coneSi->getIndices());
 
  }
}

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
CglLiftAndProject::CglLiftAndProject ()
:
CglCutGenerator(),
beta_(1),
epsilon_(1.0e-08),
onetol_(1-epsilon_)
{
  // nothing to do here
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
CglLiftAndProject::CglLiftAndProject (const CglLiftAndProject & source) :
   CglCutGenerator(source),
   beta_(source.beta_),
   epsilon_(source.epsilon_),
   onetol_(source.onetol_)
{
  // Nothing to do here
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
CglLiftAndProject::~CglLiftAndProject ()
{
  // Nothing to do here
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
CglLiftAndProject &
CglLiftAndProject::operator=(
                                         const CglLiftAndProject& rhs)
{
  if (this != &rhs) {
    CglCutGenerator::operator=(rhs);
    beta_=rhs.beta_;
    epsilon_=rhs.epsilon_;
    onetol_=rhs.onetol_;
  }
  return *this;
}
