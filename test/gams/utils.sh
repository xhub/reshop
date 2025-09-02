

usr1_trap() {
   echo "User interruption!"
   exit
}

#busybox on windows does not support USR1
case "$(uname -o)" in
   "CYGWIN_NT-10.0" | "Msys" | "MS/Windows")
       ;;

   *)
      trap usr1_trap USR1
esac


if ! command -v "pushd" &> /dev/null; then
   function pushd() {
      : "${POPD_STACK_DIRS:=}"
    export POPD_STACK_DIRS="$PWD
${POPD_STACK_DIRS}"
    echo "${POPD_STACK_DIRS}" | xargs
    cd "${1}"
   }

alias popd='LINE=`echo "\${POPD_STACK_DIRS}" | sed -ne '1p'`;[ "$LINE" != "" ] && cd $LINE; export POPD_STACK_DIRS=`echo "\${POPD_STACK_DIRS}" | sed -e '1d'`;echo "${POPD_STACK_DIRS}" | xargs'

fi

#
#Set Colors
#



if command -v "tput" &> /dev/null; then

bold=$(tput bold)
underline=$(tput sgr 0 1)
reset=$(tput sgr0)

purple=$(tput setaf 171)
red=$(tput setaf 1)
green=$(tput setaf 76)
tan=$(tput setaf 3)
blue=$(tput setaf 38)
cyan=$(tput setaf 6)

else

bold="\033[1m"
underline="\033[4m"
reset="\033[0m"

# ANSI Escape Codes for 256 colors
# The format is '\033[38;5;<color_code>m' for foreground
# and '\033[48;5;<color_code>m' for background.

# Use ANSI 256 color codes for a wide range of colors
purple=$(printf "\033[38;5;171m")
red=$(printf "\033[38;5;9m")
green=$(printf "\033[38;5;10m")
tan=$(printf "\033[38;5;3m")
blue=$(printf "\033[38;5;21m")
cyan=$(printf "\033[38;5;6m")

fi
#bgBlue=$(tput setab 4)



#
# Headers and  Logging
#

e_header() {
   local string="$1"
   local string_length="${#string}"
   local total_width=80

      # Calculate the number of padding characters needed on each side
      # We use arithmetic expansion $((...)) for integer calculations.
      local pad_length=$(( (total_width - string_length - 4) / 2 ))

      # Check if the string is too long to be padded
      if [ "$pad_length" -lt 0 ]; then
         printf "Error: The string is too long to be padded to 80 characters.\n" >&2
         return 1
      fi

      # Create the padding strings
      local pad_left=""
      local pad_right=""

      # This loop is POSIX compliant
      local i=0
      while [ "$i" -lt "$pad_length" ]; do
         pad_left="${pad_left}="
         pad_right="${pad_right}="
         i=$((i + 1))
      done

      # Handle odd string lengths to keep the total width at 80
      if [ $(( (total_width - string_length - 2) % 2 )) -ne 0 ]; then
         pad_right="${pad_right}="
      fi
      printf "\n${bold}${purple}${pad_left}  %s  ${pad_right}${reset}\n" "$@"
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

e_subcase() { printf "${cyan}➜ %s${reset}" "$@"
}

e_subcase_timed() {

   local name=$1
   local namelen=${#name}
   local sec=$2
   local seclen=${#sec}
   local padlen=$((80-5-seclen))
   
   if [ "$padlen" -lt 0 ]; then
      printf "\r${green}✔${cyan} %s %s s${reset}\n" "$name" "$sec" 
   else
      printf "\r${green}✔${cyan} %-*s %s s${reset}\n" "$padlen" "$name" "$sec"
   fi
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
      e_error "gams $gms_name $* FAILED!"
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
   e_header "$bname tests"

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
      start_time=$(date +%s)
      cd "$TMPDIR";
      exec_gams "$(basename "$f")"
      end_time=$(date +%s)
      duration=$((end_time - start_time))
      e_subcase_timed "$f" $duration
      cd ..
   done
 
# Try to be POSIX compliant
#   if [[ $# -eq 1 || ($# -ge 2 && $2 != "keep") ]]; then
#   Complex conditions are hard ...
   if [ $# -ge 2 ] && [ "$2" != "keep" ]; then
      doKeep=1
   else
      doKeep=0
   fi
   if [ $# -eq 1 ] || [ $doKeep -eq 1 ]; then
      rm -rf "$TMPDIR"
   fi
   
   popd > /dev/null
}
