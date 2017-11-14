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
prep_data=1
extract_feats=1
train_gmm=1
compile_graph=0
decode_test=0
save_model=0
#
##
###


### HYPER-PARAMETERS
##
#
tot_gauss_mono=1000
num_leaves_tri=1000
tot_gauss_tri=2000
decode_beam=13
decode_lattice_beam=7
decode_max_active_states=700
num_iters_mono=40
num_iters_tri=40
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
audio_dir="${input_dir}/audio"
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
# mfcc_dir=mfcc_${corpus_name}
plp_dir=plp_${corpus_name}
#
##
###





if [ "$prep_data" -eq "1" ]; then
    
    printf "\n####=================####\n";
    printf "#### BEGIN DATA PREP ####\n";
    printf "####=================####\n\n";

    ./prep_data.sh $input_dir $audio_dir $data_dir
    
fi



if [ "$extract_feats" -eq "1" ]; then

    printf "\n####==========================####\n";
    printf "#### BEGIN FEATURE EXTRACTION ####\n";
    printf "####==========================####\n\n";

    ./extract_feats.sh $data_dir/train $plp_dir 
    
fi



if [ "$train_gmm" -eq "1" ]; then

    printf "\n####==========================####\n";
    printf "#### BEGIN FEATURE EXTRACTION ####\n";
    printf "####==========================####\n\n";

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
    
    printf "\n####=========================####\n";
    printf "#### BEGIN GRAPH COMPILATION ####\n";
    printf "####=========================####\n\n";

    compile_graph.sh
    
fi



if [ "$decode_test" -eq "1" ]; then
    
    printf "\n####================####\n";
    printf "#### BEGIN DECODING ####\n";
    printf "####================####\n\n";

    test_gmm.sh
fi

exit;


