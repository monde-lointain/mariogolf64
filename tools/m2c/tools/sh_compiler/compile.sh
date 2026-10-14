#!/bin/sh
set -eu

if [ -f /work/input.i ]; then
    input=input.i
elif [ -f /work/input.c ]; then
    input=input.c
else
    echo "no input.c or input.i in /work" >&2
    exit 1
fi

rm -rf /tmp/sh-compile
mkdir -p /tmp/sh-compile/.dosemu
cp -a /opt/gccsh/. /tmp/sh-compile/
cp "/work/$input" "/tmp/sh-compile/$input"

cat > /tmp/sh-compile/dosemurc <<'EOF'
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_sound = (off)
$_hogthreshold = (0)
$_pci = (off)
$_rdtsc = (off)
$_term_color = (off)
$_ipxsupport = (off)
$_pktdriver = (off)
EOF

cd /tmp/sh-compile
HOME=/tmp/sh-compile dosemu -quiet -dumb -f ./dosemurc -K . \
    -E "GCC.EXE $* $input -o output.s"

if [ -s output.s ]; then
    compiler_output=output.s
elif [ -s OUTPUT.S ]; then
    compiler_output=OUTPUT.S
else
    echo "compiler path did not produce output.s" >&2
    exit 1
fi

sed 's/[[:space:]]*;#.*$//' "$compiler_output" > /work/output.s
