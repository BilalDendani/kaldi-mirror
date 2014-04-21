#!/bin/bash

# Copyright 2014 Andreas Kirkedal

# Licensed under the Apache License, Version 2.0 (the "License");                                                    
# you may not use this file except in compliance with the License.                                                  
# You may obtain a copy of the License at                                                                          
#                                                                                                                 
#  http://www.apache.org/licenses/LICENSE-2.0                                                                    
#                                                                                                               
# THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY                                 
# KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED                                   
# WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,                                       
# MERCHANTABLITY OR NON-INFRINGEMENT.                                                                                 
# See the Apache 2 License for the specific language governing permissions and                                       
# limitations under the License.

#dir=norm_dk

dos2unix $2

mode=$1

dir=$(pwd)/local/norm_dk

abbr=$dir/anot.tmp
rem=$dir/rem.tmp
line=$dir/line.tmp
num=$dir/num.tmp
nonum=$dir/nonum.tmp

$dir/expand_abbr_medical.sh $2 > $abbr;
$dir/remove_annotation.sh $abbr > $rem;
if [ $mode != "am" ]; then
    $dir/sent_split.sh $rem > $line;
else
    $dir/write_out_formatting.sh $rem > $line;
fi

$dir/expand_dates.sh $line |\
$dir/format_punct.sh  >  $num;
#python3 $dir/writenumbers.py $dir/numbersUp.tbl $num $nonum;
cat $num | $dir/write_punct.sh | \
perl -pi -e "s/^\n//" | PERLIO=:utf8 perl -pe '$_=uc'

# Comment this line for debugging
wait
#rm -f $abbr $rem $line $num
