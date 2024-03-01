#!/usr/bin/env bash

#===================================================
#
#    Copyright (c) 2018-2021,2023
#      SMASH Team
#
#    GNU General Public License (GPLv3 or later)
#
#===================================================

usage() {
cat <<END
-------------------------------------------
Executing several SMASH run benchmarks
-------------------------------------------
Note: Prepare the build directory yourself.
Clean it and compile the SMASH version you
want to benchmark.
Further, make sure to free enough disk
space as the output files require 38 GB.

Usage: $0 PREPARED_BUILD_DIR

Give '-h' or '--help' as the argument to
display this help.
-------------------------------------------
END
}

fail() {
  echo "$*" >&2
  exit 1
}

type perf  >/dev/null 2>&1 || fail "Failed to find perf (This script only works on Linux)."
type uname >/dev/null 2>&1 || fail "Failed to find uname."
type lshw  >/dev/null 2>&1 || fail "Failed to find lshw."

if [[ $# -ne 1 ]]; then
  fail "Script requires exactly one argument. Use '-h' for help."
fi

if [[ $1 == "-h" || $1 == "--help" ]]; then
  usage
  exit 0
else
  BUILD_DIR=$1
  if [[ ! -f "${BUILD_DIR}/smash" ]]; then
    fail "Given build directory \"${BUILD_DIR}\" does not contain a smash executable."
  fi
fi


SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd )"
SMASH_ROOT="${SCRIPTPATH}/../.."

cd $BUILD_DIR

echo "Collecting System Information ..."

SMASH_VER_NUM=$(./smash -v | grep -E 'SMASH-')
SMASH_VER_OUT=$(./smash -v)
HW_INFO=$(lshw -short 2> /dev/null)
UNAME_INFO=$(uname -a)


echo "Running Benchmarks for ${SMASH_VER_NUM} ..."
echo "(Runs for roughly 2h.)"


benchmark_run() {
  local YAML_DIR DECAYM_DIR PART_DIR
  YAML_DIR=$1
  DECAYM_DIR=$2
  PART_DIR=$3
  if [ "$#" -eq 4 ]
  then
    CONFIG_OPT=$4
    PERF_OUT=$(perf stat -B -r3 \
               ./smash \
               -i ${SCRIPTPATH}/configs/${YAML_DIR}/config.yaml \
               -d ${DECAYM_DIR}/decaymodes.txt \
               -p ${PART_DIR}/particles.txt \
               -c "$CONFIG_OPT" \
               2>&1 >/dev/null)
  else
    PERF_OUT=$(perf stat -B -r3 \
               ./smash \
               -i ${SCRIPTPATH}/configs/${YAML_DIR}/config.yaml \
               -d ${DECAYM_DIR}/decaymodes.txt \
               -p ${PART_DIR}/particles.txt \
               2>&1 >/dev/null)
  fi
  echo "$PERF_OUT"
}

# Defaults
PART_DEF="${SMASH_ROOT}/input"
DECAYM_DEF="${SMASH_ROOT}/input"

echo "   Started benchmark for collider ..."
coll_perf=$(benchmark_run collider $DECAYM_DEF $PART_DEF)
echo "$coll_perf" | grep -E "time elapsed"

echo "   Started benchmark for timestepless ..."
nots_perf=$(benchmark_run collider $DECAYM_DEF $PART_DEF 'General: { Time_Step_Mode: None }')
echo "$nots_perf" | grep -E "time elapsed"

echo "   Started benchmark for box ..."
box_perf=$(benchmark_run box "${SMASH_ROOT}/input/box" "${SMASH_ROOT}/input/box")
echo "$box_perf" | grep -E "time elapsed"

echo "   Started benchmark for sphere ..."
sphere_perf=$(benchmark_run sphere $DECAYM_DEF $PART_DEF)
echo "$sphere_perf" | grep -E "time elapsed"

echo "   Started benchmark for dileptons ..."
dilepton_perf=$(benchmark_run dileptons "${SMASH_ROOT}/input/dileptons" $PART_DEF)
echo "$dilepton_perf" | grep -E "time elapsed"

echo "   Started benchmark for photons ..."
photons_perf=$(benchmark_run photons "${SCRIPTPATH}/configs/photons" "${SCRIPTPATH}/configs/photons")
echo "$photons_perf" | grep -E "time elapsed"

echo "   Started benchmark for testparticles ..."
testp_perf=$(benchmark_run testparticles $DECAYM_DEF $PART_DEF)
echo "$testp_perf" | grep -E "time elapsed"

echo "   Started benchmark for potentials ..."
potentials_perf=$(benchmark_run potentials $DECAYM_DEF $PART_DEF)
echo "$potentials_perf" | grep -E "time elapsed"

echo "   Started benchmark for high-energy collisions ..."
high_energy_perf=$(benchmark_run high_energy $DECAYM_DEF $PART_DEF)
echo "$high_energy_perf" | grep -E "time elapsed"


OUTPUT_FILE=${SCRIPTPATH}/bm-results-${SMASH_VER_NUM}.md

# \` are espaced backticks
cat > ${OUTPUT_FILE}<<EOF
# Benchmark Results for $SMASH_VER_NUM

### \`smash -v\`
\`\`\`
$SMASH_VER_OUT}
\`\`\`

## System information

### \`uname -a\`
\`\`\`
$UNAME_INFO
\`\`\`

### \`lshw -short\`
\`\`\`
$HW_INFO
\`\`\`

## Results

Performed with \`perf\` and the \`stat -B\` options.

### Collider Run (AuAu@1.23)
\`\`\`
$coll_perf
\`\`\`

### Collider Run without Timesteps
Same setup as collider run, but without timesteps.
\`\`\`
$nots_perf
\`\`\`

### Box Run
\`\`\`
$box_perf
\`\`\`

### Sphere Run
\`\`\`
$sphere_perf
\`\`\`

### Dilepton Run
Same setup as collider run, but with dileptons.
\`\`\`
$dilepton_perf
\`\`\`

### Photons Run
Rho Pi Box with elastic cross section.
\`\`\`
$photons_perf
\`\`\`

### Collider Run with Testparticles (CuCu@1.23)
Baseline for Potentials Run. 20 testparticles.
\`\`\`
$testp_perf
\`\`\`

### Potentials Run
Same setup as with Testparticles, but also with potentials.
\`\`\`
$potentials_perf
\`\`\`

### High-energy collision Run
\`\`\`
$high_energy_perf
\`\`\`
EOF
echo "Results are written to $OUTPUT_FILE"
