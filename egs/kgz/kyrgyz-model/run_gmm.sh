#!/bin/bash

# Joshua Meyer (2017)


# USAGE:
#
#      ./run.sh <corpus_name>
#
# INPUT:
#
#    input_dir/
#       lexicon.txt
#       lexicon_nosil.txt
#       phones.txt
#       task.arpabo
#       transcripts
#
#       audio_dir/
#          utterance1.wav
#          utterance2.wav
#          utterance3.wav
#               .
#               .
#          utteranceN.wav
#
#    config_dir/
#       mfcc.conf
#       topo_orig.proto
#
#
# OUTPUT:
#
#    exp_dir
#    feat_dir
#    data_dir
# 


corpus_name=$1
run=$2

if [ "$#" -ne 2 ]; then
    echo "ERROR: $0"
    echo "USAGE: $0 <corpus_name> <run>"
    exit 1
fi


### STAGES
##
#
prep_train_audio=1
extract_train_feats=1
compile_Lfst=1
train_gmm=1
compile_graph=1
prep_test_audio=1
extract_test_feats=1
decode_test=1
#
##
###


### HYPER-PARAMETERS
##
#
tot_gauss_mono=500
num_leaves_tri=500
tot_gauss_tri=1000
num_iters_mono=10
num_iters_tri=10
#
##
###


### SHOULD ALREADY EXIST
##
#
num_processors=$(nproc)
unknown_word="<unk>"
unknown_phone="SPOKEN_NOISE"
silence_phone="SIL"
input_dir=input_${corpus_name}
train_audio_dir="${input_dir}/audio"
config_dir=config
cmd="utils/run.pl"
#
##
###


### GENERATED BY SCRIPT
##
#
data_dir=data_${corpus_name}
exp_dir=exp_${corpus_name}
plp_dir=plp_${corpus_name}
#
##
###




if [ "$prep_train_audio" -eq "1" ]; then

    printf "\n####==========================####\n";
    printf "#### TRAINING AUDIO DATA PREP ####\n";
    printf "####==========================####\n\n";

    local/prepare_audio_data.sh \
        /data/downsampled/train \
        /data/downsampled/transcripts.train \
        $data_dir \
        train
fi



if [ "$extract_train_feats" -eq "1" ]; then

    printf "\n####==========================####\n";
    printf "#### TRAIN FEATURE EXTRACTION ####\n";
    printf "####==========================####\n\n";

    ./extract_feats.sh $data_dir/train $plp_dir $num_processors

fi




if [ "$compile_Lfst" -eq "1" ]; then
    
    printf "\n####==============####\n";
    printf "#### Create L.fst ####\n";
    printf "####==============####\n\n";

    ./compile_Lfst.sh $input_dir $data_dir
    
fi


if [ "$train_gmm" -eq "1" ]; then

    printf "\n####===============####\n";
    printf "#### TRAINING GMMs ####\n";
    printf "####===============####\n\n";

    ./train_gmm.sh \
        $data_dir \
        $num_iters_mono \
        $tot_gauss_mono \
        $num_iters_tri \
        $tot_gauss_tri \
        $num_leaves_tri \
        $exp_dir;
fi



if [ "$compile_graph" -eq "1" ]; then
    
    printf "\n####===================####\n";
    printf "#### GRAPH COMPILATION ####\n";
    printf "####===================####\n\n";

    utils/mkgraph.sh \
        $input_dir \
        $data_dir \
        $data_dir/lang_decode \
        $exp_dir/triphones/graph \
        exp_org/triphones/tree \
        exp_org/triphones/final.mdl \
        || printf "\n####\n#### ERROR: mkgraph.sh \n####\n\n" \
        || exit 1;

fi




if [ "$prep_test_audio" -eq "1" ]; then

    printf "\n####==========================####\n";
    printf "#### TESTING AUDIO DATA PREP ####\n";
    printf "####==========================####\n\n";

    local/prepare_audio_data.sh \
        /data/downsampled/test \
        /data/downsampled/transcripts.test \
        $data_dir \
        test
fi



if [ "$extract_test_feats" -eq "1" ]; then

    printf "\n####=========================####\n";
    printf "#### TEST FEATURE EXTRACTION ####\n";
    printf "####=========================####\n\n";

    ./extract_feats.sh $data_dir/test $plp_dir $num_processors
    
fi




if [ "$decode_test" -eq "1" ]; then
    
    printf "\n####================####\n";
    printf "#### BEGIN DECODING ####\n";
    printf "####================####\n\n";

    suffix=${corpus_name}_${run}
    
    ./test_gmm.sh \
        $exp_dir/triphones/graph/HCLG.fst \
        $exp_dir/triphones/final.mdl \
        $data_dir/test \
        $suffix \
        $num_processors;
    


fi

exit;


