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

for d in "$(dirname "$0")"/*/; do
   if [[ -f "$d"/test.sh ]]; then 
      bash "$d"/test.sh
   fi
done
