usr1_trap() {
   echo "User interruption!"
   exit
}

trap usr1_trap USR1

#
#Set Colors
#

bold=$(tput bold)
underline=$(tput sgr 0 1)
reset=$(tput sgr0)

purple=$(tput setaf 171)
red=$(tput setaf 1)
green=$(tput setaf 76)
tan=$(tput setaf 3)
blue=$(tput setaf 38)
cyan=$(tput setaf 6)

#bgBlue=$(tput setab 4)

#
# Headers and  Logging
#

e_header() { printf "\n${bold}${purple}==========  %s  ==========${reset}\n" "$@"
}
e_arrow() { printf "➜ %s\n" "$@"
}
e_success() { printf "${green}✔ %s${reset}\n" "$@"
}
e_error() { printf "${red}✖ %s${reset}\n" "$@"
}
e_warning() { printf "${tan}➜ %s${reset}\n" "$@"
}
e_underline() { printf "${underline}${bold}%s${reset}\n" "$@"
}
e_bold() { printf "${bold}%s${reset}\n" "$@"
}
e_note() { printf "${underline}${bold}${blue}Note:${reset}  ${blue}%s${reset}\n" "$@"
}
e_subcase() { printf "${cyan}➜ %s${reset}\n" "$@"
}

: "${EMPSLV:=reshopdev}"

: "${GMSLO:=2}"

exec_gams() {
   local gms_name=$1
   shift
   set +e
   env RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 gams "${gms_name}" lo="$GMSLO" keep=1 optfile=1 emp="$EMPSLV" "$@"
   local status=$?
   if [ $status != 0 ]; then
      e_error "${gms_name} with args has failed: status = ${status}"
      env RHP_LOG=all gams "${gms_name}" lo=4 keep=1 optfile=1 emp="$EMPSLV" "$@" 
      [ -z ${NO_EXIT_EARLY+x} ] && exit 1;
   fi
   set -e
}


run_all_gms() {
   if [[ $# -eq 0 ]]; then
      echo "Illegal number of parameters: need at least 1" >&2
      exit 2
   fi
   pushd "$1" > /dev/null || exit 1

   bname="$(basename "$1")"
   e_header "Running tests in $bname"

   TMPDIR=tmp_testxx-"$EMPSLV"
   rm -rf "$TMPDIR"
   mkdir "$TMPDIR"

   # To silence GAMS
   export DEBUG_PGAMS=0
   
   # Start tests
   for f in *.gms
   do
      cp "$f" "$TMPDIR"
      e_subcase "Running $f"
      cd "$TMPDIR";
      exec_gams "$(basename "$f")"
      cd ..
   done
   
   if [[ $# -eq 1 || ($# -ge 2 && $2 != "keep") ]]; then
      rm -rf "$TMPDIR"
   fi
   
   popd > /dev/null
}
