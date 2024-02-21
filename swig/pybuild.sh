#!/bin/bash

set -euf -o pipefail

function compile_reshop() {
   pushd "$1"

   cmake .
   make -j3
   patchelf --set-rpath '$ORIGIN' libreshop.so

   popd
}

BUILD_DIR=${1:-/scratch/build/reshop/}

#PYTHONS="python3.9 python3.8 /home/xhub/sdcard/miniconda37/bin/python3.7"
PYTHONS="python3 /scratch/miniconda/3.8/bin/python3"
RESHOP_SRCDIR=$(dirname $(realpath $(dirname $0)))/src
export RESHOP_SRCDIR

if [ ! -f setup.py ]; then
   echo "ERROR: setup.py is not in the cwd"
   exit 1
fi

[[ -f "$BUILD_DIR"/libreshop.so ]] || compile_reshop "$BUILD_DIR"

rm -rf reshop
mkdir reshop
cp __init__.py reshop

RESHOP_FILES=$(lddtree -l "${BUILD_DIR}"/libreshop.so | grep "${BUILD_DIR}")
FILES="$RESHOP_FILES ${BUILD_DIR}/libpath50.so"
for f in $FILES; do
   cp -f "$f" reshop/
done

rm -rf dist
for PYTHON in $PYTHONS
do
   rm -rf build
	# "${PYTHON}" -m build
	"${PYTHON}" setup.py bdist_wheel
   PYTHON_VER=$("${PYTHON}" -V | sed -e 's:.*\(3\.[^\.]*\)\..*:\1:')
   rm -rf "build-${PYTHON_VER}"
   mv build "build-${PYTHON_VER}"
done


exit
