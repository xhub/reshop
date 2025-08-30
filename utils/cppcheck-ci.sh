#!/bin/sh

cppcheck \
-j $(grep "cpu cores" /proc/cpuinfo | uniq | tail -c 2) \
--enable=all \
--inconclusive \
--force \
--quiet \
--xml \
--inline-suppr \
--output-file=report-cppcheck.xml \
--project=compile_commands_reshop-only.json \
-iapifiles \
-iexternal/ppl_lcdd.cpp \
--check-level=exhaustive \
-DGAMS_UNUSED= \
-D_GNU_SOURCE \
-UDO_POISON \
-U_WIN32 \
-U_MSC_VER \
-URHP_OUTPUT_BACKTRACE \
-URHP_EXTRA_DEBUG_OUTPUT \
-URHP_DEBUGGER_STOP \
-URHP_DEV_MODE \
-URHP_EXPERIMENTAL \
-UNO_DELETE_2024_12_18 \
-UGITLAB_83 \
-UWITH_BACKWARD \
-UWITH_FMEM \
-UWITH_HDF5 \
-UUNUSED_ON_20240320 \
-UUNUSED_2025_05_16 \
-UDEBUG_OVF \
-UDEBUG_OVF_CONJUGATE \
-URCTR_FULL_DEBUG \
-U__INTEL_COMPILER \
-U__ICL


mkdir cppcheck

cppcheck-htmlreport --source-dir=.. --title=ReSHOP --file=report-cppcheck.xml --report-dir=cppcheck
