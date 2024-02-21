#!/bin/bash - 
#===============================================================================
#
#          FILE: test.sh
# 
#         USAGE: ./test.sh 
# 
#   DESCRIPTION: 
# 
#        AUTHOR: Olivier Huber (oli.huber@gmail.com), 
#===============================================================================


set -euo pipefail                              # Treat unset variables as an error

dir="$(dirname "$0")"
. "$(dirname "$dir")"/utils.sh

RHP_NO_STOP=1
RHP_NO_BACKTRACE=1
export RHP_NO_STOP RHP_NO_BACKTRACE


NOCOMP=${NOCOMP:+1}
NOFAIL=${NOFAIL:+1}


run_all_gms "$dir"
