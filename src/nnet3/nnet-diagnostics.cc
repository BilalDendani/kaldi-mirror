// nnet3/nnet-diagnostics.cc

// Copyright      2015    Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
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

#include "nnet3/nnet-diagnostics.h"
#include "nnet3/nnet-utils.h"

namespace kaldi {
namespace nnet3 {

NnetComputeProb::NnetComputeProb(const NnetComputeProbOptions &config,
                                 const Nnet &nnet):
    config_(config),
    nnet_(nnet),
    compiler_(nnet),
    num_minibatches_processed_(0) { }


void NnetComputeProb::Compute(const NnetExample &eg) {
  bool need_model_derivative = false,
      store_component_stats = false;
  ComputationRequest request;
  GetComputationRequest(nnet_, eg, need_model_derivative,
                        store_component_stats,
                        &request);
  const NnetComputation *computation = compiler_.Compile(request);
  NnetComputer computer(config_.compute_config, *computation,
                        nnet_, NULL);
  // give the inputs to the computer object.
  computer.AcceptInputs(nnet_, eg);
  computer.Forward();
  this->ProcessOutputs(eg, &computer);
}

void NnetComputeProb::ProcessOutputs(const NnetExample &eg,
                                     NnetComputer *computer) {
  std::vector<NnetIo>::const_iterator iter = eg.io.begin(),
      end = eg.io.end();
  for (; iter != end; ++iter) {
    const NnetIo &io = *iter;
    int32 node_index = nnet_.GetNodeIndex(io.name);
    ObjectiveType obj_type = nnet_.GetNode(node_index).u.objective_type;
    KALDI_ASSERT(node_index >= 0);
    if (nnet_.IsOutputNode(node_index)) {
      const CuMatrixBase<BaseFloat> &output = computer->GetOutput(io.name);
      if (output.NumCols() != io.features.NumCols()) {
        KALDI_ERR << "Nnet versus example output dimension (num-classes) "
                  << "mismatch for '" << io.name << "': " << output.NumCols()
                  << " (nnet) vs. " << io.features.NumCols() << " (egs)\n";
      }
      {
        BaseFloat tot_weight, tot_objf;
        bool supply_deriv = false;      
        ComputeObjectiveFunction(io.features, obj_type, io.name,
                                 supply_deriv, computer,
                                 &tot_weight, &tot_objf);
        SimpleObjectiveInfo &totals = objf_info_[io.name];       
        totals.tot_weight += tot_weight;
        totals.tot_objective += tot_objf;
      }
      if (obj_type == kLinear) {
        BaseFloat tot_weight, tot_accuracy;
        ComputeAccuracy(io.features, output,
                        &tot_weight, &tot_accuracy);
        SimpleObjectiveInfo &totals = accuracy_info_[io.name];
        totals.tot_weight += tot_weight;
        totals.tot_objective += tot_accuracy;
      }
      num_minibatches_processed_++;
    }
  }
}

bool NnetComputeProb::PrintTotalStats() const {
  bool ans = false;
  unordered_map<std::string, SimpleObjectiveInfo, StringHasher>::const_iterator
      iter, end;
  { // First print regular objectives  
    iter = objf_info_.begin();
    end = objf_info_.end();
    for (; iter != end; ++iter) {
      const std::string &name = iter->first;
      int32 node_index = nnet_.GetNodeIndex(name);
      KALDI_ASSERT(node_index >= 0);
      ObjectiveType obj_type = nnet_.GetNode(node_index).u.objective_type;
      const SimpleObjectiveInfo &info = iter->second;
      KALDI_LOG << "Overall "
                << (obj_type == kLinear ? "log-likelihood" : "objective")
                << " for '" << name << "' is "
                << (info.tot_objective / info.tot_weight) << " per frame"
                << ", over " << info.tot_weight << " frames.";
      if (info.tot_weight > 0)
        ans = true;
    }
  }
  { // now print accuracies.
    iter = accuracy_info_.begin();
    end = accuracy_info_.end();
    for (; iter != end; ++iter) {
      const std::string &name = iter->first;
      const SimpleObjectiveInfo &info = iter->second;
      KALDI_LOG << "Overall accuracy for '" << name << "'is "
                << (info.tot_objective / info.tot_weight) << " per frame"
                << ", over " << info.tot_weight << " frames.";
      // don't bother changing ans; the loop over the regular objective should
      // already have set it to true if we got any data.
    }
  }
  return ans;
}

void ComputeAccuracy(const GeneralMatrix &supervision,
                     const CuMatrixBase<BaseFloat> &nnet_output,
                     BaseFloat *tot_weight_out,
                     BaseFloat *tot_accuracy_out) {
  int32 num_rows = nnet_output.NumRows(),
      num_cols = nnet_output.NumCols();
  KALDI_ASSERT(supervision.NumRows() == num_rows &&
               supervision.NumCols() == num_cols);

  CuArray<int32> best_index(num_rows);
  nnet_output.FindRowMaxId(&best_index);
  std::vector<int32> best_index_cpu;
  // wasteful copy, but doesn't dominate.
  best_index.CopyToVec(&best_index_cpu);


  double tot_weight = 0.0,
      tot_accuracy = 0.0;

  // note: we expect that in most cases where this code is called,
  // supervision.Type() will be kSparseMatrix.
  switch (supervision.Type()) {
    case kCompressedMatrix: {
      Matrix<BaseFloat> mat;
      supervision.GetMatrix(&mat);
      for (int32 r = 0; r < num_rows; r++) {
        SubVector<BaseFloat> vec(mat, r);
        BaseFloat row_sum = vec.Sum();
        KALDI_ASSERT(row_sum >= 0.0);
        int32 best_index;
        vec.Max(&best_index);  // discard max value.
        tot_weight += row_sum;
        if (best_index == best_index_cpu[r])
          tot_accuracy += row_sum;
      }
      break;
      
    }
    case kFullMatrix: {
      const Matrix<BaseFloat> &mat = supervision.GetFullMatrix();
      for (int32 r = 0; r < num_rows; r++) {
        SubVector<BaseFloat> vec(mat, r);
        BaseFloat row_sum = vec.Sum();
        KALDI_ASSERT(row_sum >= 0.0);
        int32 best_index;
        vec.Max(&best_index);  // discard max value.
        tot_weight += row_sum;
        if (best_index == best_index_cpu[r])
          tot_accuracy += row_sum;
      }
      break;
    }
    case kSparseMatrix: {
      const SparseMatrix<BaseFloat> &smat = supervision.GetSparseMatrix();
      for (int32 r = 0; r < num_rows; r++) {
        const SparseVector<BaseFloat> &row = smat.Row(r);
        BaseFloat row_sum = row.Sum();
        int32 best_index;
        row.Max(&best_index);
        KALDI_ASSERT(best_index < num_cols);
        tot_weight += row_sum;
        if (best_index == best_index_cpu[r])
          tot_accuracy += row_sum;
      }
      break;
    }
    default: KALDI_ERR << "Bad general-matrix type.";
  }
  *tot_weight_out = tot_weight;
  *tot_accuracy_out = tot_accuracy;
}
      
} // namespace nnet3
} // namespace kaldi
