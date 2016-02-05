// nnet3/nnet-general-component.h

// Copyright      2015  Johns Hopkins University (author: Daniel Povey)

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

#ifndef KALDI_NNET3_NNET_GENERAL_COMPONENT_H_
#define KALDI_NNET3_NNET_GENERAL_COMPONENT_H_

#include "nnet3/nnet-common.h"
#include "nnet3/nnet-component-itf.h"
#include "nnet3/natural-gradient-online.h"
#include <iostream>

namespace kaldi {
namespace nnet3 {

/// @file  This file contains declarations of components that are not "simple",
///   meaning they care about the indexes they are operating on, don't return
///   the kSimpleComponent flag in their Properties(), and may return a different
///   number of outputs than inputs.



/**
   This Component takes a larger input-dim than output-dim, where the input-dim
   must be a multiple of the output-dim, and distributes different blocks of the
   input dimension to different 'x' values.  In the normal case where the input
   is only valid at x=0, the first block of output goes to x=0, the second block
   at x=1, and so on.  It also supports a more general usage, so in general a
   value 'x' at the output will map to block 'x % n_blocks' of the dimension
   blocks of the input, and to an x value 'x / n_blocks' of the input.  For negative
   x values the % and / operations are always rounded down, not towards zero.

   The config line is of the form
     input-dim=xx output-dim=xx
   where input-dim must be a multiple of the output-dim, and n_blocks is
   set to input-dim / output-dim.
   */
class DistributeComponent: public Component {
 public:
  DistributeComponent(int32 input_dim, int32 output_dim) {
    Init(input_dim, output_dim);
  }
  DistributeComponent(): input_dim_(0), output_dim_(0) { }
  virtual int32 InputDim() const { return input_dim_; }
  virtual int32 OutputDim() const { return output_dim_; }

  // use the default Info() function.
  virtual void InitFromConfig(ConfigLine *cfl);
  virtual std::string Type() const { return "DistributeComponent"; }
  virtual int32 Properties() const { return kLinearInInput; }
  virtual void Propagate(const ComponentPrecomputedIndexes *indexes,
                         const CuMatrixBase<BaseFloat> &in,
                         CuMatrixBase<BaseFloat> *out) const;
  virtual void Backprop(const std::string &debug_info,
                        const ComponentPrecomputedIndexes *indexes,
                        const CuMatrixBase<BaseFloat> &in_value,
                        const CuMatrixBase<BaseFloat> &out_value,
                        const CuMatrixBase<BaseFloat> &out_deriv,
                        Component *, // to_update,
                        CuMatrixBase<BaseFloat> *in_deriv) const;

  virtual void Read(std::istream &is, bool binary); // This Read function
  // requires that the Component has the correct type.

  /// Write component to stream
  virtual void Write(std::ostream &os, bool binary) const;
  virtual Component* Copy() const {
    return new DistributeComponent(input_dim_, output_dim_);
  }


  // Some functions that are only to be reimplemented for GeneralComponents.
  virtual void GetInputIndexes(const MiscComputationInfo &misc_info,
                               const Index &output_index,
                               std::vector<Index> *desired_indexes) const;

  // This function returns true if at least one of the input indexes used to
  // compute this output index is computable.
  virtual bool IsComputable(const MiscComputationInfo &misc_info,
                            const Index &output_index,
                            const IndexSet &input_index_set,
                            std::vector<Index> *used_inputs) const;

  virtual ComponentPrecomputedIndexes* PrecomputeIndexes(
      const MiscComputationInfo &misc_info,
      const std::vector<Index> &input_indexes,
      const std::vector<Index> &output_indexes,
      bool need_backprop) const;

  // Some functions that are specific to this class.
  void Init(int32 input_dim, int32 output_dim);
 private:
  // computes the input index corresponding to a particular output index.
  // if block != NULL, also computes which block of the input this corresponds to.
  inline void ComputeInputIndexAndBlock(const Index &output_index,
                                        Index *input_index,
                                        int32 *block) const;
  inline void ComputeInputPointers(
      const ComponentPrecomputedIndexes *indexes,
      const CuMatrixBase<BaseFloat> &in,
      int32 num_output_rows,
      std::vector<const BaseFloat*> *input_pointers) const;
  // non-const version of the above.
  inline void ComputeInputPointers(
      const ComponentPrecomputedIndexes *indexes,
      int32 num_output_rows,
      CuMatrixBase<BaseFloat> *in,
      std::vector<BaseFloat*> *input_pointers) const;
  int32 input_dim_;
  int32 output_dim_;

};

/*
  Class StatisticsExtractionComponent is used together with
  StatisticsPoolingComponent to extract moving-average mean and
  standard-deviation statistics.

  StatisticsExtractionExomponent designed to extract statistics-- 0th-order,
  1st-order and optionally diagonal 2nd-order stats-- from small groups of
  frames, such as 10 frame.  The statistics will then be further processed by
  StatisticsPoolingComponent to compute moving-average means and (if configured)
  standard deviations.  The reason for the two-component way of doing this is
  efficiency, particularly in the graph-compilation phase.  (Otherwise there
  would be too many dependencies to process).  The StatisticsExtractionComponent
  is designed to let you extract statistics from fixed-size groups of frames
  (e.g. 10 frames), and in StatisticsPoolingComponent you are only expected to
  compute the averages at the same fixed period (e.g. 10 frames), so it's more
  efficient than if you were to compute a moving average at every single frame;
  and the computation of the intermediate stats means that most of the
  computation that goes into extracting the means and standard deviations for
  nearby frames is shared.

  The config line in a typical setup will be something like:

    input-dim=250 input-period=1 output-period=10 include-variance=true

  input-dim is self-explanatory.  The inputs will be obtained at multiples of
  input-period (e.g. it might be 3 for chain models).  output-period must be a
  multiple of input period, and the requested output indexes will be expected to
  be multiples of output-period (which you can ensure through use of the Round
  descriptor).  For instance, if you request the output on frame 80, it will
  consist of stats from input frames 80 through 89.

  An output of this component will be 'computable' any time at least one of
  the corresponding inputs is computable.

   In all cases the first dimension of the output will be a count (between 1 and
  10 inclusive in this example).  If include-variance=false, then the output
  dimension will be input-dim + 1.  and the output dimensions >0 will be
  1st-order statistics (sums of the input).  If include-variance=true, then the
  output dimension will be input-dim * 2 + 1, where the raw diagonal 2nd-order
  statistics are appended to the 0 and 1st order statistics.

  The default configuration values are:
     input-dim=-1 input-period=1 output-period=1 include-variance=true
 */
class StatisticsExtractionComponent: public Component {
 public:
  // Initializes to defaults which would not pass Check(); use InitFromConfig()
  // or Read() or copy constructor to really initialize.
  StatisticsExtractionComponent();
  // copy constructor, used in Copy().
  StatisticsExtractionComponent(const StatisticsExtractionComponent &other);

  virtual int32 InputDim() const { return input_dim_; }
  virtual int32 OutputDim() const {
    // count + sum stats [ + sum-squared stats].
    return 1 + input_dim_ + (include_variance_ ? input_dim_ : 0);
  }
  virtual void InitFromConfig(ConfigLine *cfl);
  virtual std::string Type() const { return "StatisticsExtractionComponent"; }
  virtual int32 Properties() const {
    return kPropagateAdds|kReordersIndexes|
        (include_variance_ ? kBackpropNeedsInput : 0);
  }
  virtual void Propagate(const ComponentPrecomputedIndexes *indexes,
                         const CuMatrixBase<BaseFloat> &in,
                         CuMatrixBase<BaseFloat> *out) const;
  virtual void Backprop(const std::string &debug_info,
                        const ComponentPrecomputedIndexes *indexes,
                        const CuMatrixBase<BaseFloat> &in_value,
                        const CuMatrixBase<BaseFloat> &out_value,
                        const CuMatrixBase<BaseFloat> &out_deriv,
                        Component *, // to_update,
                        CuMatrixBase<BaseFloat> *in_deriv) const;

  virtual void Read(std::istream &is, bool binary); // This Read function
  // requires that the Component has the correct type.

  /// Write component to stream
  virtual void Write(std::ostream &os, bool binary) const;
  virtual Component* Copy() const {
    return new StatisticsExtractionComponent(*this);
  }

  // Some functions that are only to be reimplemented for GeneralComponents.
  virtual void GetInputIndexes(const MiscComputationInfo &misc_info,
                               const Index &output_index,
                               std::vector<Index> *desired_indexes) const;

  virtual bool IsComputable(const MiscComputationInfo &misc_info,
                            const Index &output_index,
                            const IndexSet &input_index_set,
                            std::vector<Index> *used_inputs) const;

  // This function reorders the input and output indexes so that they
  // are sorted first on n and then x and then t.
  virtual void ReorderIndexes(std::vector<Index> *input_indexes,
                              std::vector<Index> *output_indexes) const;

  virtual ComponentPrecomputedIndexes* PrecomputeIndexes(
      const MiscComputationInfo &misc_info,
      const std::vector<Index> &input_indexes,
      const std::vector<Index> &output_indexes,
      bool need_backprop) const;

 private:
  // Checks that the parameters are valid.
  void Check() const;

  // Disallow assignment operator.
  StatisticsExtractionComponent &operator =(
      const StatisticsExtractionComponent &other);

  int32 input_dim_;
  int32 input_period_;
  int32 output_period_;
  bool include_variance_;
};


/*
  Class StatisticsPoolingComponent is used together with
  StatisticsExtractionComponent to extract moving-average mean and
  standard-deviation statistics.

  StatisticsPoolingComponent pools the stats over a specified window and
  computes means and possibly log-count and stddevs from them for you.

 # In StatisticsPoolingComponent, the first element of the input is interpreted
 # as a count, which we divide by.
 # Optionally the log of the count can be output, and you can allow it to be
 # repeated several times if you want (useful for systems using the jesus-layer).
 # The output dimension is equal to num-log-count-features plus (input-dim - 1).

 # If include-log-count==false, the output dimension is the input dimension minus one.
 # If output-stddevs=true, then it expects the input-dim to be of the form 2n+1 where n is
 #  presumably the original feature dim, and it interprets the last n dimensions of the feature
 #  as a variance; it outputs the square root of the variance instead of the actual variance.

 configs and their defaults:  input-dim=-1, input-period=1, left-context=-1, right-context=-1,
    num-log-count-features=0, output-stddevs=true, variance-floor=1.0e-10

 You'd access the output of the StatisticsPoolingComponent using rounding, e.g.
  Round(component-name, 10)
 or whatever, instead of just component-name, because its output is only defined at multiples
 of its input-period.

 The output of StatisticsPoolingComponent will only be defined if at least one input was defined.
 */
class StatisticsPoolingComponent: public Component {
 public:
  // Initializes to defaults which would not pass Check(); use InitFromConfig()
  // or Read() or copy constructor to really initialize.
  StatisticsPoolingComponent();
  // copy constructor, used in Copy()
  StatisticsPoolingComponent(const StatisticsPoolingComponent &other);

  virtual int32 InputDim() const { return input_dim_; }
  virtual int32 OutputDim() const {
    return input_dim_ + num_log_count_features_ - 1;
  }
  virtual void InitFromConfig(ConfigLine *cfl);
  virtual std::string Type() const { return "StatisticsPoolingComponent"; }
  virtual int32 Properties() const {
    return kReordersIndexes|kBackpropAdds|
        (output_stddevs_ || num_log_count_features_ > 0 ?
         kBackpropNeedsOutput : 0) |
        (num_log_count_features_ == 0 ? kBackpropNeedsInput : 0);
  }
  virtual void Propagate(const ComponentPrecomputedIndexes *indexes,
                         const CuMatrixBase<BaseFloat> &in,
                         CuMatrixBase<BaseFloat> *out) const;
  virtual void Backprop(const std::string &debug_info,
                        const ComponentPrecomputedIndexes *indexes,
                        const CuMatrixBase<BaseFloat> &in_value,
                        const CuMatrixBase<BaseFloat> &out_value,
                        const CuMatrixBase<BaseFloat> &out_deriv,
                        Component *, // to_update,
                        CuMatrixBase<BaseFloat> *in_deriv) const;

  virtual void Read(std::istream &is, bool binary); // This Read function
  // requires that the Component has the correct type.

  /// Write component to stream
  virtual void Write(std::ostream &os, bool binary) const;
  virtual Component* Copy() const {
    return new StatisticsPoolingComponent(*this);
  }

  // Some functions that are only to be reimplemented for GeneralComponents.
  virtual void GetInputIndexes(const MiscComputationInfo &misc_info,
                               const Index &output_index,
                               std::vector<Index> *desired_indexes) const;

  // returns true if at least one of its inputs is computable.
  virtual bool IsComputable(const MiscComputationInfo &misc_info,
                            const Index &output_index,
                            const IndexSet &input_index_set,
                            std::vector<Index> *used_inputs) const;

  // This function reorders the input and output indexes so that they
  // are sorted first on n and then x and then t.
  virtual void ReorderIndexes(std::vector<Index> *input_indexes,
                              std::vector<Index> *output_indexes) const;

  virtual ComponentPrecomputedIndexes* PrecomputeIndexes(
      const MiscComputationInfo &misc_info,
      const std::vector<Index> &input_indexes,
      const std::vector<Index> &output_indexes,
      bool need_backprop) const;

 private:
  // Checks that the parameters are valid.
  void Check() const;

  // Disallow assignment operator.
  StatisticsPoolingComponent &operator =(
      const StatisticsPoolingComponent &other);

  int32 input_dim_;
  int32 input_period_;
  int32 left_context_;
  int32 right_context_;
  int32 num_log_count_features_;
  bool output_stddevs_;
  BaseFloat variance_floor_;
};



} // namespace nnet3
} // namespace kaldi


#endif
