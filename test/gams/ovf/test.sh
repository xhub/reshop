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

NOCOMP=${NOCOMP:+1}
NOFAIL=${NOFAIL:+1}

: "${EMPSLV:=reshopdev}"
: "${GMSLO:=2}"

NUMDIFF_TOL=2e-10

status=0

exec_gams() {
   local gms_name=$1
   shift
   env RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 gams "${gms_name}" lo="$GMSLO" keep=1 optfile=1 emp="$EMPSLV" "$@"
   status=$?
   if [ $status != 0 ]; then
      e_error "$1 with args has failed: status = ${status}"
      set +x
      env RHP_NO_STOP=1 RHP_NO_BACKTRACE=1 gams "${gms_name}" lo=4 keep=1 optfile=1 emp="$EMPSLV" "$@"
      env RHP_LOG=all gams "${gms_name}" lo=4 keep=1 optfile=1 emp="$EMPSLV" "$@"
      set -x
      e_error "gams $gms_name $* FAILED!"
      [ -z ${NO_EXIT_EARLY+x} ] && exit 1;
   fi
}


pushd "$(dirname "$0")" || exit 1

TMPDIR=tmp_testxx-"$EMPSLV"

rm -rf "$TMPDIR"
mkdir "$TMPDIR"

cp -r *.gdx ref *.inc "$TMPDIR"/

#QSF="huber l1 l2 elastic_net hinge vapnik soft_hinge hubnik"
QSF="huber l1 l2 elastic_net vapnik hubnik huber_scaled hubnik_scaled"
LOSS_GMS="test_loss.gms test_loss1.gms"

# To silence GAMS
export DEBUG_PGAMS=0

# Start tests


for i in ${LOSS_GMS}
do
    cp "${i}" "$TMPDIR"
    e_header "Testing ${i}"
    for qsf in $QSF
    do 
        e_subcase "OVF is ${qsf}"
        start_time=$(date +%s)
        cd "$TMPDIR";
        exec_gams "$(basename "${i}")" --qs_function="$qsf"
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        e_subcase_timed "OVF is ${qsf}" $duration
        cd ..
    done
done

cp test_loss_nl.gms "$TMPDIR"
e_header "Testing test_loss_nl.gms"
for qsf in $QSF
do
    cd "$TMPDIR";
    e_subcase "OVF is ${qsf}"
    start_time=$(date +%s)
    for v in $(seq 1 5)
    do
        exec_gams test_loss_nl.gms qcp=cplex --qs_function="$qsf" --version="$v"
    done
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    e_subcase_timed "OVF is ${qsf}" $duration
    cd ..
done

pushd "$TMPDIR"/test_res > /dev/null
if command -v numdiff &> /dev/null; then
if [ -z "$NOCOMP" ]; then
    [ -z "$NOFAIL" ] && set -e
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
    [ -z "$NOFAIL" ] && set +e
fi
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
    cp "${i}" "$TMPDIR"
    e_header "Testing ${i}"
    for ovf_method in $OVF_METHODS
    do
        cd "$TMPDIR";
        start_time=$(date +%s)
        e_subcase "${ovf_method}"
        exec_gams $(basename "${i}") --ovf_method="${ovf_method}" --nomacro=1
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        e_subcase_timed "${ovf_method}" $duration
        if [ $status != 0 ]; then exit $status; fi
        cd ..
    done
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

