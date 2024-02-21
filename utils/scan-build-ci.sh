#!/bin/sh

LLVM_VER=17

scan-build-$LLVM_VER --use-cc=clang-$LLVM_VER --use-c++=clang++-$LLVM_VER -o scan-build cmake -DBUILD_GAMS_SOLVER=0 -DWITH_BACKWARD=0 -DWITH_BACKTRACE=0 -DGIT_HASH=$CI_COMMIT_SHORT_SHA $@

# We used to be able to do 1300 ...
scan-build-$LLVM_VER --use-cc=clang-$LLVM_VER --use-c++=clang++-$LLVM_VER -maxloop 300 -o scan-build --exclude apifiles/C/api -enable-checker \
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


# core.AdjustedReturnValue,\
# core.AttributeNonNull,\

