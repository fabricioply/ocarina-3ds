#!/bin/sh
# Build checks for ocarina.3gx: undefined symbols, VFP instructions, section sizes, worst stack frames.
# Run from the repository root after `make` (DEVKITARM must be set).
: "${DEVKITARM:?DEVKITARM is not set}"
P=$DEVKITARM/bin
cd "$(dirname "$0")/.." || exit 1
ls -l ocarina.3gx ocarina.elf
echo "--- nm -u (must be empty)"
$P/arm-none-eabi-nm -u ocarina.elf
echo "--- VFP instruction count (must be 0)"
$P/arm-none-eabi-objdump -d ocarina.elf | grep -E '^ +[0-9a-f]+:' | awk '{print $3}' | grep -cE '^(v[a-z]|fmrx|fmxr|fmstat)'
echo "--- sections"
$P/arm-none-eabi-size -A ocarina.elf | grep -E 'text|rodata|data|bss|Total'
echo "--- largest stack frames (bytes)"
cat *.su | awk -F'\t' '{print $2"\t"$1}' | sort -n -r | head -12
echo "--- end symbol"
$P/arm-none-eabi-nm ocarina.elf | grep -E ' (__end__|g_ig|g_lb)$'
