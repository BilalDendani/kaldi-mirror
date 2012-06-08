// cudamatrix/cu-rand.h

// Copyright 2012  Karel Vesely

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.



#ifndef KALDI_CUDAMATRIX_CURAND_H_
#define KALDI_CUDAMATRIX_CURAND_H_


#include "cudamatrix/cu-matrix.h"


namespace kaldi {


template<typename Real> 
class CuRand {
 public:

  CuRand()
   : z1_(NULL), z2_(NULL), z3_(NULL), z4_(NULL), state_size_(0),
     host_(NULL), host_size_(0)
  { }

  ~CuRand() { }

  /// on demand seeding of all the buffers
  void SeedGpu(MatrixIndexT state_size);

  /// fill with uniform random numbers (0.0-1.0)
  void RandUniform(CuMatrix<Real> *tgt);
  /// fill with normal random numbers
  void RandGaussian(CuMatrix<Real> *tgt);

  /// align probabilities to discrete 0/1 states (use uniform samplig)
  void BinarizeProbs(const CuMatrix<Real> &probs, CuMatrix<Real> *states);
  /// add gaussian noise to each element
  void AddGaussNoise(CuMatrix<Real> *tgt, Real gscale = 1.0);

 private:
  /// seed one buffer
  void SeedBuffer(uint32* *tgt, MatrixIndexT state_size);
   
 private:

  // CANNOT DEFINE CuMatrix<uint32>, 
  // CuMatrix has back-off Matrix which cannot hold integers. 
  // The inner state of random number generator will be in 
  // a raw buffer with the size of the current matrix. 
  //
  // Use on-demand seeding to get the correct size of the buffer.
  
  /// Inner state of the ``grid-like'' random number generator
  uint32 *z1_, *z2_, *z3_, *z4_; 
  int32 state_size_; ///< size of the buffers

  uint32 *host_; ///< host bufer, used for initializing
  int32 host_size_; ///< size of the host buffer

  CuMatrix<Real> tmp;
};



/// declare the BaseFloat specializations, that are in cu-rand.cc
template<> void CuRand<float>::RandUniform(CuMatrix<float> *tgt);
template<> void CuRand<float>::RandGaussian(CuMatrix<float> *tgt);
template<> void CuRand<float>::BinarizeProbs(const CuMatrix<float> &probs, CuMatrix<float> *states);
template<> void CuRand<float>::AddGaussNoise(CuMatrix<float> *tgt, float gscale);


/// also define the non-specialized versions, so the code always compies
template<typename Real> void CuRand<Real>::RandUniform(CuMatrix<Real> *tgt) {
  KALDI_ERR << __func__ << " Not implemented"; 
}
template<typename Real> void CuRand<Real>::RandGaussian(CuMatrix<Real> *tgt) {
  KALDI_ERR << __func__ << " Not implemented"; 
}
template<typename Real> void CuRand<Real>::BinarizeProbs(const CuMatrix<Real> &probs, CuMatrix<Real> *states) {
  KALDI_ERR << __func__ << " Not implemented"; 
}
template<typename Real> void CuRand<Real>::AddGaussNoise(CuMatrix<Real> *tgt, Real gscale) {
  KALDI_ERR << __func__ << " Not implemented";
} 




} // namsepace




#endif
