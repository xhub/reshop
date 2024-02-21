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
-UWITH_BACKWARD \
-UWITH_FMEM \
-DGAMS_UNUSED=

mkdir cppcheck

cppcheck-htmlreport --source-dir=.. --title=ReSHOP --file=report-cppcheck.xml --report-dir=cppcheck
