#!/usr/bin/env bash
# run_all_combos.sh
# Runs ttp-multi for every combination of:
#   - strategy pairs       (all ordered pairs from STRATEGIES list)
#   - noise                (0, 1)
#   - whale_enabled        (0, 1)
#   - whale_mult           (3, 5, 7)          — only when whale_enabled=1
#   - whale_prob           (0.05, 0.10, 0.20) — only when whale_enabled=1
#
# When whale_enabled=0, mult and prob are fixed at 0 / 0.0 (ignored by sim).
# Each configuration gets its own output subdirectory.
#
# Usage:
#   ./run_all_combos.sh [exe] [jobs]
#
# Defaults:
#   exe  = ./ttp-multi
#   jobs = 4
#
# Examples:
#   ./run_all_combos.sh
#   ./run_all_combos.sh ./build/ttp-multi 16

set -euo pipefail

# ---------------------------------------------------------------------------
# Arguments with defaults
# ---------------------------------------------------------------------------
EXE="${1:-./ttp-multi}"
JOBS="${2:-4}"

# ---------------------------------------------------------------------------
# Strategy list
# ---------------------------------------------------------------------------
STRATEGIES=(
    "selfish"
    "stubborn-trail"
    "stubborn-fork"
    "stubborn-lead"
    "stubborn-lead-fork"
    "stubborn-trail-fork"
    "stubborn-lead-trail"
    "stubborn-lead-trail-fork"
    "petty"
    "publish-3"
    "publish-4"
)

# ---------------------------------------------------------------------------
# Parameter sweeps
# ---------------------------------------------------------------------------
NOISE_VALS=(0 1)
WHALE_ENABLED_VALS=(0 1)
WHALE_MULT_VALS=(3 5 7)
WHALE_PROB_VALS=(0.05 0.10 0.20)

# ---------------------------------------------------------------------------
# Build the full job list.
# Each line: S1 S2 NOISE WHALE_ENABLED WHALE_MULT WHALE_PROB OUTDIR
# ---------------------------------------------------------------------------
JOBS_FILE=$(mktemp)
trap 'rm -f "$JOBS_FILE"' EXIT

for NOISE in "${NOISE_VALS[@]}"; do
    NOISE_TAG=$([ "$NOISE" -eq 1 ] && echo "noise" || echo "no-noise")

    for WHALE_ENABLED in "${WHALE_ENABLED_VALS[@]}"; do

        if [[ "$WHALE_ENABLED" -eq 0 ]]; then
            # No whale — single config; mult/prob are irrelevant to the sim
            OUTDIR="results/${NOISE_TAG}/no-whale"
            mkdir -p "$OUTDIR"

            for S1 in "${STRATEGIES[@]}"; do
                for S2 in "${STRATEGIES[@]}"; do
                    printf '%s %s %s %s %s %s %s\n' \
                        "$S1" "$S2" "$NOISE" "$WHALE_ENABLED" "0" "0.0" "$OUTDIR" \
                        >> "$JOBS_FILE"
                done
            done

        else
            # Whale enabled — sweep mult × prob
            for WHALE_MULT in "${WHALE_MULT_VALS[@]}"; do
                for WHALE_PROB in "${WHALE_PROB_VALS[@]}"; do
                    # e.g. 0.05 → p05,  0.10 → p10,  0.20 → p20
                    PROB_TAG=$(printf '%s' "$WHALE_PROB" | sed 's/0\.\([0-9]*\)/p\1/')
                    OUTDIR="results/${NOISE_TAG}/whale-mult${WHALE_MULT}-prob${PROB_TAG}"
                    mkdir -p "$OUTDIR"

                    for S1 in "${STRATEGIES[@]}"; do
                        for S2 in "${STRATEGIES[@]}"; do
                            printf '%s %s %s %s %s %s %s\n' \
                                "$S1" "$S2" "$NOISE" "$WHALE_ENABLED" \
                                "$WHALE_MULT" "$WHALE_PROB" "$OUTDIR" \
                                >> "$JOBS_FILE"
                        done
                    done
                done
            done
        fi

    done
done

TOTAL=$(wc -l < "$JOBS_FILE")

echo "============================================"
echo " ttp-multi — full parameter sweep"
echo "============================================"
printf " Executable   : %s\n"   "$EXE"
printf " Noise vals   : %s\n"   "${NOISE_VALS[*]}"
printf " Whale mult   : %s\n"   "${WHALE_MULT_VALS[*]}"
printf " Whale prob   : %s\n"   "${WHALE_PROB_VALS[*]}"
printf " Strategies   : %d  →  %d ordered pairs\n" \
    "${#STRATEGIES[@]}" "$(( ${#STRATEGIES[@]} * ${#STRATEGIES[@]} ))"
printf " Parallel     : %d workers\n" "$JOBS"
printf " Total runs   : %d\n"   "$TOTAL"
echo "============================================"
echo ""

# ---------------------------------------------------------------------------
# Worker function — runs one sim, logs to per-run file
# ---------------------------------------------------------------------------
run_combo() {
    local S1="$1" S2="$2" NOISE="$3" WHALE_EN="$4" MULT="$5" PROB="$6" OUTDIR="$7"
    local SUFFIX="${S1}_vs_${S2}"
    local LOGFILE="${OUTDIR}/${SUFFIX}.log"
    local START END STATUS

    START=$(date +%s)

    if "$EXE" "${OUTDIR}/${SUFFIX}" \
              "$S1" "$S2" \
              "$NOISE" "$WHALE_EN" "$PROB" "$MULT" \
              > "$LOGFILE" 2>&1; then
        STATUS="OK     "
    else
        STATUS="FAILED "
    fi

    END=$(date +%s)
    printf "[%s] %s  %-25s vs %-25s  noise=%s whale=%s mult=%-2s prob=%s  (%ds)\n" \
        "$(date '+%H:%M:%S')" "$STATUS" "$S1" "$S2" \
        "$NOISE" "$WHALE_EN" "$MULT" "$PROB" "$(( END - START ))"
}

export -f run_combo
export EXE

# ---------------------------------------------------------------------------
# Dispatch — prefer GNU parallel, fall back to xargs -P
# ---------------------------------------------------------------------------
WALL_START=$(date +%s)

if command -v parallel &>/dev/null; then
    echo "Dispatcher: GNU parallel ($JOBS workers)"
    parallel --colsep ' ' -j "$JOBS" \
        run_combo {1} {2} {3} {4} {5} {6} {7} \
        < "$JOBS_FILE"
else
    echo "Dispatcher: xargs -P $JOBS  (install GNU parallel for better load balancing)"
    # Use input redirect instead of -a so this works on both macOS (BSD) and Linux
    < "$JOBS_FILE" xargs -P "$JOBS" -L 1 bash -c \
        'run_combo $1 $2 $3 $4 $5 $6 $7' _
fi

WALL_END=$(date +%s)
WALL_ELAPSED=$(( WALL_END - WALL_START ))

echo ""
echo "============================================"
printf " All %d runs finished in %ds\n" "$TOTAL" "$WALL_ELAPSED"
echo " Results in: results/"
echo "============================================"

# ---------------------------------------------------------------------------
# Directory tree summary
# ---------------------------------------------------------------------------
echo ""
echo "Output layout:"
if command -v tree &>/dev/null; then
    tree -d results/
else
    find results/ -type d | sort | sed 's|results/||; s|[^/]*/|  |g'
fi