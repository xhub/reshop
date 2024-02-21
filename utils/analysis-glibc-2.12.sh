#!/bin/bash
maxver=2.12
prefix=${1:-/lib64}
headerf=${2:-glibc.h}
set -e
for lib in libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 libresolv.so.2 librt.so.1; do
objdump -T ${prefix}/$lib
done | awk -v maxver=${maxver} -vheaderf=${headerf} -vredeff=${headerf}.redef -f <(cat <<'EOF'
BEGIN {
split(maxver, ver, /\./)
limit_ver = ver[1] * 10000 + ver[2]*100 + ver[3]
}
/GLIBC_/ {
gsub(/\(|\)/, "",$(NF-1))
split($(NF-1), ver, /GLIBC_|\./)
vers = ver[2] * 10000 + ver[3]*100 + ver[4]
if (vers > 0) {
    if (symvertext[$(NF)] != $(NF-1))
        count[$(NF)]++
    if (vers <= limit_ver && vers > symvers[$(NF)]) {
        symvers[$(NF)] = vers
        symvertext[$(NF)] = $(NF-1)
    }
}
}
END {
for (s in symvers) {
    if (count[s] > 1) {
        printf("__asm__(\".symver %s,%s@%s\");\n", s, s, symvertext[s]) > headerf
    }
}
}
EOF
)
sort ${headerf} -o ${headerf}
