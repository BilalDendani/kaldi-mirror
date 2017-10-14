#!/bin/bash

# Author Joshua Meyer (2016)
# This run script is based on other Kaldi run scripts from the egs/ dir


### HYPER-PARAMETERS
##
#

tot_gauss_mono=500
num_leaves_tri=500
tot_gauss_tri=1000


decode_beam=13 # beam for decoding. Was 13 in the scripts
decode_lattice_beam=7  # this has most effect on size of the lattices
decode_max_active_states=700 # default 700

#
##
###

num_processors=$(nproc)
unknown_word="<unk>"
unknown_phone="SPOKEN_NOISE"
silence_phone="SIL"

### THESE SHOULD ALREADY EXIST
##
#
input_dir=input_babel
babel_data_dir=/home/josh/Desktop/IARPA_BABEL_BP_105/conversational/
config_dir=config
cmd="utils/run.pl"
#
##
###

### THESE ARE GENERATED BY THE SCRIPT
##
#
exp_dir=exp_babel
mfcc_dir=mfcc_babel
plp_dir=plp_babel
data_dir=data_babel
train_dir=train
test_dir=test
#
##
###


# export PATH variable
. ./path.sh

printf "\n####======================================####\n";
printf "#### BEGIN DATA + LEXICON + LANGUAGE PREP ####\n";
printf "####======================================####\n\n";


echo "### Subsetting the TRAIN set ###"

train_data_list=/home/josh/git/kaldi-mirror/egs/babel/s5d/conf/lists/105-turkish/train.LimitedLP.list
local/babel/make_corpus_subset.sh "${babel_data_dir}/training" "$train_data_list" ./${data_dir}/raw_train_data
train_data_dir=`utils/make_absolute.sh ./${data_dir}/raw_train_data`

echo "### Preparing lexicon in data/local ###"

# Convert a Babel-formatted dictionary to work with Kaldi, and optionally
# add non-speech "words" that appear in the transcription. e.g. <laughter>

# Copy and paste existing phonetic dictionary, language model, and phone list

dict_dir=${data_dir}/local/dict
mkdir -p $dict_dir

local/babel/prepare_lexicon.pl \
    --oov "<unk>" \
    ${input_dir}/lexicon.txt \
    ${dict_dir}

all_phones=();
while IFS='' read -r line || [[ -n "$line" ]]; do
    words=( $line ); unset words[0];
    all_phones+=( "${words[@]}" );
done < ${dict_dir}/lexicon.txt;
echo "${all_phones[@]}" | tr ' ' '\n' | sort -u | tr '\n' '\n' \
                                                     > ${dict_dir}/phones.txt

# cat every non-silence phone from phones.txt into a new file
cat ${dict_dir}/phones.txt | \
    grep -v $silence_phone \
         > ${dict_dir}/nonsilence_phones.txt

echo $silence_phone > $dict_dir/silence_phones.txt
echo $silence_phone > $dict_dir/optional_silence.txt

printf "### Dictionary preparation succeeded ###"



printf "#### Preparing data in data/train ####";

local/babel/prepare_acoustic_training_data.pl \
    --vocab ${input_dir}/lexicon.txt \
    --fragmentMarkers \-\*\~ \
    $train_data_dir \
    ${data_dir}/train \
    || printf "\n####\n#### ERROR: prepare_acoustic_training_data.pl \n####\n\n" \
    || exit 1;

    
# Prepare a dir such as data/lang/
# This script can add word-position-dependent phones, and constructs a host of
# other derived files, that go in data/lang/.
# This creates the FST for the lexicon.

local/prepare_lang.sh \
    --position-dependent-phones false \
    ${data_dir}/local/dict \
    ${data_dir}/local/lang \
    ${data_dir}/lang \
    $unknown_word \
    || printf "\n####\n#### ERROR: prepare_lang.sh\n####\n\n" \
    || exit 1;


# Create the FST (G.fst) for the grammar

cp $input_dir/task.arpabo ${data_dir}/local/lm.arpa

local/prepare_lm.sh \
    $data_dir \
    || printf "\n####\n#### ERROR: prepare_lm.sh\n####\n\n" \
    || exit 1;

printf "\n####============####\n";
printf "#### END DATA PREP ####\n";
printf "####===============####\n\n";






printf "\n####==========================####\n";
printf "#### BEGIN FEATURE EXTRACTION ####\n";
printf "####==========================####\n\n";


printf "#### PLPs ####\n";

for dir in train; do

    steps/make_plp.sh \
        --cmd $cmd \
        --nj $num_processors \
        ${data_dir}/${dir} \
        ${exp_dir}/make_plp_log/${dir} \
        $plp_dir
    
    utils/fix_data_dir.sh ${data_dir}/${dir}
    
    steps/compute_cmvn_stats.sh \
        ${data_dir}/${dir} \
        ${exp_dir}/make_plp_log/${dir} \
        $plp_dir
    
    utils/fix_data_dir.sh ${data_dir}/${dir}

done


printf "\n####========================####\n";
printf "#### END FEATURE EXTRACTION ####\n";
printf "####========================####\n\n";





printf "\n####===========================####\n";
printf "#### BEGIN TRAINING MONOPHONES ####\n";
printf "####===========================####\n\n";

steps/train_mono.sh \
    --cmd "$cmd" \
    --nj $num_processors \
    --num-iters 10 \
    --totgauss $tot_gauss_mono \
    --beam 6 \
    ${data_dir}/${train_dir} \
    ${data_dir}/lang \
    ${exp_dir}/monophones \
    || printf "\n####\n#### ERROR: train_mono.sh \n####\n\n" \
    || exit 1;


../../../src/gmmbin/gmm-info ${exp_dir}/monophones/final.mdl


printf "#### BEGIN ALIGN MONOPHONES ####\n";


# Align monophones with data (si == speaker independent)

steps/align_si.sh \
    --cmd "$cmd" \
    --nj $num_processors \
    --boost-silence 1.25 \
    --beam 10 \
    --retry-beam 40 \
    ${data_dir}/${train_dir} \
    ${data_dir}/lang \
    ${exp_dir}/monophones \
    ${exp_dir}/monophones_aligned \
    || printf "\n####\n#### ERROR: align_si.sh \n####\n\n" \
    || exit 1;



printf "\n####==========================####\n";
printf "#### BEGIN TRAINING TRIPHONES ####\n";
printf "####==========================####\n\n";


printf "### Train Context Dependent Triphones ###\n"

steps/train_deltas.sh \
    --cmd "$cmd" \
    --num-iters 10 \
    --beam 10 \
    $num_leaves_tri \
    $tot_gauss_tri \
    ${data_dir}/${train_dir} \
    ${data_dir}/lang \
    ${exp_dir}/monophones_aligned \
    ${exp_dir}/triphones \
    || printf "\n####\n#### ERROR: train_deltas.sh \n####\n\n" \
    || exit 1;

../../../src/gmmbin/gmm-info ${exp_dir}/triphones/final.mdl



printf "### Align Context Dependent Triphones ###\n"

steps/align_si.sh --cmd "$cmd" \
    --nj $num_processors \
    --boost-silence 1.25 \
    --beam 10 \
    --retry-beam 40 \
    ${data_dir}/${train_dir} \
    ${data_dir}/lang \
    ${exp_dir}/triphones \
    ${exp_dir}/triphones_aligned \
    || printf "\n####\n#### ERROR: align_si.sh \n####\n\n" \
    || exit 1;


