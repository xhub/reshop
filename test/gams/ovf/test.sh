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

set -o nounset                              # Treat unset variables as an error

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
e_arrow() { printf "➜ $@\n"
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

NOCOMP=${NOCOMP:+1}
NOFAIL=${NOFAIL:+1}

: "${EMPSLV:=reshopdev}"

NUMDIFF_TOL=2e-10

status=0

exec_gams() {
   local gms_name=$1
   shift
   env RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 gams "${gms_name}" lo=2 keep=1 optfile=1 emp="$EMPSLV" "$@"
   status=$?
   if [ $status != 0 ]; then
      e_error "$1 with args has failed: status = ${status}"
      env RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 gams "${gms_name}" lo=4 keep=1 optfile=1 emp="$EMPSLV" "$@"
      env RHP_LOG=all gams "${gms_name}" lo=4 keep=1 optfile=1 "$@"
      e_error "gams $gms_name $* FAILED!"
      [ -z ${NO_EXIT_EARLY+x} ] && exit 1;
   fi
}


pushd "$(dirname "$0")" || exit 1

rm -rf tmp_testxx
mkdir tmp_testxx

cp -r *.gdx ref *.inc tmp_testxx/

#QSF="huber l1 l2 elastic_net hinge vapnik soft_hinge hubnik"
QSF="huber l1 l2 elastic_net vapnik hubnik huber_scaled hubnik_scaled"
LOSS_GMS="test_loss.gms test_loss1.gms"

# To silence GAMS
export DEBUG_PGAMS=0

# Start tests


for i in ${LOSS_GMS}
do
    cp "${i}" tmp_testxx
    e_header "Testing ${i}"
    for qsf in $QSF
    do 
        e_subcase "OVF is ${qsf}"
        cd tmp_testxx;
        exec_gams "$(basename "${i}")" --qs_function="$qsf"
        cd ..
    done
done

cp test_loss_nl.gms tmp_testxx
e_header "Testing test_loss_nl.gms"
for qsf in $QSF
do
    cd tmp_testxx;
    e_subcase "OVF is ${qsf}"
    for v in $(seq 1 5)
    do
        exec_gams test_loss_nl.gms qcp=cplex --qs_function="$qsf" --version="$v"
    done
    cd ..
done

pushd tmp_testxx/test_res > /dev/null
numdiff -h > /dev/null
if [[ $? == 0 && -z $NOCOMP ]]; then
    [[ -z $NOFAIL ]] && set -e
    for qsf in $QSF
    do
        numdiff -qr $NUMDIFF_TOL mcp_${qsf}_v1.out fenchel_${qsf}_v1.out
        numdiff -qr $NUMDIFF_TOL mcp_${qsf}_v1_fit.out fenchel_${qsf}_v1_fit.out
        for v in $(seq 2 5)
        do
            numdiff -qr $NUMDIFF_TOL mcp_${qsf}_v1.out mcp_${qsf}_v${v}.out
            numdiff -qr $NUMDIFF_TOL mcp_${qsf}_v1_fit.out mcp_${qsf}_v${v}_fit.out
            numdiff -qr $NUMDIFF_TOL fenchel_${qsf}_v1.out fenchel_${qsf}_v${v}.out
            numdiff -qr $NUMDIFF_TOL fenchel_${qsf}_v1_fit.out fenchel_${qsf}_v${v}_fit.out
        done
    done
    [[ -z $NOFAIL ]] && set +e
fi
popd > /dev/null

# temporary (famous last word) hack: libvrepr is not easily available right now

if [ -z ${HAS_VREPR+x} ]; then
   OVF_METHODS="equilibrium fenchel"
else
   OVF_METHODS="equilibrium fenchel conjugate"
fi

for i in $(ls -1 Risk*_ecvar.gms cvarup_OVF.gms crm-multistage{,_max}.gms) # crm-multistage_labels.gms)
do
    cp "${i}" tmp_testxx
    e_header "Testing ${i}"
    for ovf_method in $OVF_METHODS
    do
        cd tmp_testxx;
        e_subcase "${ovf_method}"
        exec_gams $(basename "${i}") --ovf_method="${ovf_method}" --nomacro=1
        if [ $status != 0 ]; then exit $status; fi
        cd ..
    done
done

if [[ "$#" -eq 0 || ("$#" -ge 1 && $1 != "keep") ]]; then
    rm -rf tmp_testxx
fi

popd > /dev/null

