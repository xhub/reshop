#!/bin/bash


GAMSSYSDIR=$(dirname "$(readlink -f "$(which gams)")")
PATHFILE="$GAMSSYSDIR"/libpath50.so

SAN=("Asan" "Msan" "UBsan")

pushd /scratch/build || exit 1

for s in "${SAN[@]}"
do
  rm -rf reshop-"${s}"
  mkdir reshop-"${s}"
  pushd reshop-"${s}" || exit 1

  CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE="${s}" -DGAMSSYSDIR="${GAMSSYSDIR}" -DWITH_BACKTRACE=0 -DWITH_BACKWARD=0 ~/reshop
  make -j8
  cp "${PATHFILE}" .
  LD_LIBRARY_PATH=$(pwd) ctest

  popd || exit 1
done

popd || return
