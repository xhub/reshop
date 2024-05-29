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
--project=compile_commands.json \
-iapifiles \
-iexternal/ppl_lcdd.cpp \
--check-level=exhaustive \
-DGAMS_UNUSED= \
-UDO_POISON \
-URHP_OUTPUT_BACKTRACE \
-URHP_EXTRA_DEBUG_OUTPUT \
-URHP_DEBUGGER_STOP \
-UWITH_BACKWARD \
-UWITH_FMEM \


mkdir cppcheck

cppcheck-htmlreport --source-dir=.. --title=ReSHOP --file=report-cppcheck.xml --report-dir=cppcheck
