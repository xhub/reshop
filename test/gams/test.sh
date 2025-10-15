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

if [ -n "${RHP_LDPATH:-}" ]; then
case "$(uname -o)" in
   "Darwin") export DYLD_FALLBACK_LIBRARY_PATH="$RHP_LDPATH";
       ;;

   *) ;;
esac


   export 
fi

for d in "$(dirname "$0")"/*/; do
   if [[ -f "$d"/test.sh ]]; then 
      exec "$d"/test.sh
   fi
done
