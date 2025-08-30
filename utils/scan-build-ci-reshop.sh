#!/bin/sh

LLVM_MVER=21

scan-build-$LLVM_MVER --use-cc=clang-$LLVM_MVER --use-c++=clang++-$LLVM_MVER -o scan-build \
   cmake -DBUILD_GAMS_DRIVER=0 -DWITH_BACKWARD=0 -DWITH_BACKTRACE=0 -DGIT_HASH=$CI_COMMIT_SHORT_SHA -DBUILD_IMGUI=0 "$@"

# We used to be able to do 1300 ...
scan-build-$LLVM_MVER --use-cc=clang-$LLVM_MVER --use-c++=clang++-$LLVM_MVER -maxloop 300 -o scan-build --exclude apifiles/C/api -enable-checker \
deadcode.DeadStores,\
nullability.NullableDereferenced,\
nullability.NullablePassedToNonnull,\
nullability.NullableReturnedFromNonnull,\
optin.performance.Padding,\
optin.portability.UnixAPI,\
security.FloatLoopCounter,\
valist.CopyToSelf,\
valist.Uninitialized,\
valist.Unterminated \
make -j $(grep "cpu cores" /proc/cpuinfo | uniq | tail -c 2)

# optin.core.EnumCastOutOfRange

# core.AdjustedReturnValue,\
# core.AttributeNonNull,\

