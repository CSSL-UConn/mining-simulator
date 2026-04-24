#!/usr/bin/env python3

"""
watcher.py
Claude/ directory agent watcher for mining simulator.

Watches for sim_pause.lock, computes a state summary from rolling files, calls Claude API with cached knowledge.md + compact state summary,
writes strategy_update.csv if a strategy change is warranted,
then removes sim_pause.lock to unblock the sim.
"""

import os
import sys
import time
import logging
import csv
import re
import math
import anthropic
from datetime import datetime
from pathlib import Path

_env_path = Path(__file__).parent / ".env"
if _env_path.exists():
    for line in _env_path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            os.environ.setdefault(k.strip(), v.strip())

CLAUDE_DIR    = Path(__file__).parent.resolve()
LOCK_FILE     = CLAUDE_DIR / "sim_pause.lock"
STRATEGY_OUT  = CLAUDE_DIR / "strategy_update.csv"
AGENT_LOG     = CLAUDE_DIR / "agent_log.csv"
KNOWLEDGE_MD  = CLAUDE_DIR / "knowledge.md"

SIM_OUTPUT_BASE = "agentTest"  # match your sim's output argument
EPOCH_ROLLING = CLAUDE_DIR / f"epoch_rolling.csv"
BLOCK_ROLLING = CLAUDE_DIR / f"block_rolling.csv"

POLL_INTERVAL  = 0.5   # seconds between lock-file polls
API_TIMEOUT    = 30    # seconds before giving up on Claude API call
MODEL          = "claude-sonnet-4-6"
MAX_TOKENS     = 50    # output is one CSV line or nothing

TOTAL_EPOCHS   = 12    # constant — change if sim config changes

VALID_STRATEGIES = {
     "selfish", 
    "stubborn-trail", "stubborn-fork", "stubborn-lead",
    "stubborn-lead-fork", "stubborn-trail-fork",
    "stubborn-lead-trail", "stubborn-lead-trail-fork",
    "petty", "lazy-fork",  
    "publish-3", "publish-4"
}

INCENTIVE_PATTERN = re.compile(
    r"^incentive-trail-(1|2)(-lead)?(-fork)?-(\d+(\.\d+)?)$"
)


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(CLAUDE_DIR / "watcher.log", mode="a"),
    ]
)
log = logging.getLogger(__name__)


def init_agent_log():
    if not AGENT_LOG.exists():
        with open(AGENT_LOG, "w") as f:
            f.write("timestamp,epoch,decision,strategy\n")

def append_agent_log(epoch: int, decision: str, strategy: str):
    with open(AGENT_LOG, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([datetime.utcnow().isoformat(), epoch, decision, strategy])

def get_last_strategy() -> str:
    """Read the most recent applied strategy from agent_log."""
    if not AGENT_LOG.exists():
        return "unknown"
    last = "unknown"
    try:
        with open(AGENT_LOG) as f:
            for row in csv.DictReader(f):
                if row.get("decision") == "applied" and row.get("strategy"):
                    last = row["strategy"]
    except Exception:
        pass
    return last

# utils 
def read_csv_rows(path: Path) -> list[dict]:
    """Return list of row dicts from a CSV file, skipping comment lines."""
    rows = []
    try:
        with open(path) as f:
            lines = [l for l in f if not l.startswith("#")]
        reader = csv.DictReader(lines)
        for row in reader:
            rows.append({k.strip(): v.strip() for k, v in row.items()})
    except Exception as e:
        log.warning(f"Could not read {path}: {e}")
    return rows

def safe_float(val: str, default: float = 0.0) -> float:
    try:
        return float(val)
    except Exception:
        return default

def safe_int(val: str, default: int = 0) -> int:
    try:
        return int(val)
    except Exception:
        return default


def find_col(row: dict, *candidates) -> str:
    # CSV helper file
    for c in candidates:
        if c in row:
            return row[c]

    for key in row:
        for c in candidates:
            if key.endswith(c):
                return row[key]
    return "0"

def compute_state_summary() -> tuple[str, int]:
    #Generate summary information to give prompt (epoch + block level data)
    epoch_rows = read_csv_rows(EPOCH_ROLLING)
    block_rows = read_csv_rows(BLOCK_ROLLING)

    if not epoch_rows:
        return "No epoch data available yet.", -1


    e_last  = epoch_rows[-1]
    e_prev  = epoch_rows[-2] if len(epoch_rows) >= 2 else None

    current_epoch   = safe_int(find_col(e_last, "dap_index"))
    epochs_remaining = max(0, TOTAL_EPOCHS - current_epoch - 1)

    total_blocks  = safe_float(find_col(e_last, "total_blocks_on_chain"))
    total_mined   = safe_float(find_col(e_last, "total_blocks_mined"))
    orphan_rate   = (1.0 - total_blocks / total_mined) if total_mined > 0 else 0.0

    spb_last = safe_float(find_col(e_last, "seconds_per_block"))
    spb_prev = safe_float(find_col(e_prev, "seconds_per_block")) if e_prev else spb_last
    spb_trend = "rising" if spb_last > spb_prev * 1.01 else \
                "falling" if spb_last < spb_prev * 0.99 else "stable"

    # Miner 0 epoch metrics
    m0_atk_last  = safe_float(find_col(e_last,  "Miner-0_atk_revenue", "_atk_revenue"))
    m0_cf_last   = safe_float(find_col(e_last,  "Miner-0_honest_cf",   "_honest_cf"))
    m0_ra_last   = safe_float(find_col(e_last,  "Miner-0_ra_this_dap", "_ra_this_dap"))
    m0_cum_last  = safe_float(find_col(e_last,  "Miner-0_cumulative_ra","_cumulative_ra"))
    m0_rrr_last  = safe_float(find_col(e_last,  "Miner-0_rrr",         "_rrr"))
    m0_blk_last  = safe_float(find_col(e_last,  "Miner-0_atk_blocks",  "_atk_blocks"))

    m0_ra_prev   = safe_float(find_col(e_prev,  "Miner-0_ra_this_dap", "_ra_this_dap")) if e_prev else 0.0
    m0_cum_prev  = safe_float(find_col(e_prev,  "Miner-0_cumulative_ra","_cumulative_ra")) if e_prev else 0.0
    m0_rrr_prev  = safe_float(find_col(e_prev,  "Miner-0_rrr",         "_rrr")) if e_prev else 0.0

    # Miner 1 epoch metrics
    m1_ra_last   = safe_float(find_col(e_last,  "Miner-1_ra_this_dap", "_ra_this_dap"))
    m1_cum_last  = safe_float(find_col(e_last,  "Miner-1_cumulative_ra","_cumulative_ra"))
    m1_rrr_last  = safe_float(find_col(e_last,  "Miner-1_rrr",         "_rrr"))
    m1_ra_prev   = safe_float(find_col(e_prev,  "Miner-1_ra_this_dap", "_ra_this_dap")) if e_prev else 0.0
    m1_rrr_prev  = safe_float(find_col(e_prev,  "Miner-1_rrr",         "_rrr")) if e_prev else 0.0

    # Estimated alpha from block fraction
    alpha_est = (m0_blk_last / total_blocks) if total_blocks > 0 else 0.0

    # Cumulative RA trend
    cum_ra_delta = m0_cum_last - m0_cum_prev
    cum_trend = "rising" if cum_ra_delta > 0 else "falling" if cum_ra_delta < 0 else "flat"

    # Per-epoch RA trend
    epoch_ra_trend = "improving" if m0_ra_last > m0_ra_prev else \
                     "worsening" if m0_ra_last < m0_ra_prev else "stable"

    # Revenue variance (whale signal) from block_rolling
    whale_flag = ""
    if block_rows:
        values = [safe_float(r.get("block_value", "0")) for r in block_rows]
        if len(values) > 10:
            mean = sum(values) / len(values)
            variance = sum((v - mean) ** 2 for v in values) / len(values)
            cv = math.sqrt(variance) / mean if mean > 0 else 0
            if cv > 0.5:
                whale_flag = f"HIGH (cv={cv:.2f}) — treat RA signals with caution"
            else:
                whale_flag = f"normal (cv={cv:.2f})"


    within_epoch_trend = ""
    if len(block_rows) >= 20:
        recent  = [safe_float(r.get("Miner-0_atk_rev_this_dap", "0"))
                   for r in block_rows[-10:]]
        earlier = [safe_float(r.get("Miner-0_atk_rev_this_dap", "0"))
                   for r in block_rows[-20:-10]]
        recent_avg  = sum(recent)  / len(recent)
        earlier_avg = sum(earlier) / len(earlier)
        within_epoch_trend = "accelerating" if recent_avg > earlier_avg * 1.05 else \
                             "decelerating" if recent_avg < earlier_avg * 0.95 else "steady"

    current_strategy = get_last_strategy()

    lines = [
        f"=== SIMULATION STATE — Epoch {current_epoch} ===",
        f"Epochs remaining: {epochs_remaining} of {TOTAL_EPOCHS}",
        f"Miner 0 current strategy: {current_strategy}",
        f"Estimated alpha: {alpha_est:.3f}",
        f"",
        f"--- Miner 0 ---",
        f"Per-epoch RA:     epoch {current_epoch-1 if e_prev else '?'}={m0_ra_prev:+.1f}  "
        f"epoch {current_epoch}={m0_ra_last:+.1f}  ({epoch_ra_trend})",
        f"Cumulative RA:    prev={m0_cum_prev:+.1f}  now={m0_cum_last:+.1f}  "
        f"delta={cum_ra_delta:+.1f}  trend={cum_trend}",
        f"RRR:              prev={m0_rrr_prev:.3f}  now={m0_rrr_last:.3f}",
        f"",
        f"--- Miner 1 (strategy unknown) ---",
        f"Per-epoch RA:     prev={m1_ra_prev:+.1f}  now={m1_ra_last:+.1f}",
        f"Cumulative RA:    {m1_cum_last:+.1f}",
        f"RRR:              prev={m1_rrr_prev:.3f}  now={m1_rrr_last:.3f}",
        f"",
        f"--- Network ---",
        f"Orphan rate:      {orphan_rate:.3f}",
        f"Seconds/block:    {spb_last:.1f}  ({spb_trend})",
        f"Revenue variance: {whale_flag}",
    ]

    if within_epoch_trend:
        lines.append(f"Within-epoch trend: {within_epoch_trend}")

    return "\n".join(lines), current_epoch


CSV_PATTERN = re.compile(
    r"^\s*0\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([a-z0-9_.()-]+)\s*$",
    re.IGNORECASE
)

def parse_response(text: str):
   # Ensure strategy is valid to simulator
    for line in text.strip().splitlines():
        line = line.strip()
        if not line:
            continue
        m = CSV_PATTERN.match(line)
        if m:
            start_block = int(m.group(1))
            end_block   = int(m.group(2))
            strategy    = m.group(3).lower()
            if strategy in VALID_STRATEGIES or INCENTIVE_PATTERN.match(strategy):
                return start_block, end_block, strategy
            else:
                log.warning(f"Unknown strategy in response: '{strategy}' — ignoring")
                return None
    return None


def call_claude(state_summary: str) -> str:
    knowledge = KNOWLEDGE_MD.read_text()

    client = anthropic.Anthropic(timeout=API_TIMEOUT)

    message = client.messages.create(
        model=MODEL,
        max_tokens=MAX_TOKENS,
        system=[
            {
                "type": "text",
                "text": knowledge,
                "cache_control": {"type": "ephemeral"}
            }
        ],
        messages=[
            {
                "role": "user",
                "content": (
                    "Current simulation state:\n\n"
                    f"{state_summary}\n\n"
                    "Based on the data above and your knowledge base, "
                    "output a strategy line for miner 0 or nothing."
                )
            }
        ]
    )

    usage = message.usage
    if hasattr(usage, "cache_read_input_tokens"):
        log.info(f"Token usage — input: {usage.input_tokens}, "
                 f"cache_read: {usage.cache_read_input_tokens}, "
                 f"cache_write: {getattr(usage, 'cache_creation_input_tokens', 0)}, "
                 f"output: {usage.output_tokens}")

    return message.content[0].text if message.content else ""

def process_epoch():
    state_summary, epoch_idx = compute_state_summary()
    log.info(f"Epoch {epoch_idx} state:\n{state_summary}")
    log.info("Calling Claude...")

    raw_response = ""
    try:
        raw_response = call_claude(state_summary)
        log.info(f"Claude response: {repr(raw_response)}")
    except anthropic.APITimeoutError:
        log.warning(f"Claude API timed out after {API_TIMEOUT}s, no strategy change.")
        append_agent_log(epoch_idx, "timeout", "")
        return
    except Exception as e:
        log.warning(f"Claude API error: {e}, no strategy change.")
        append_agent_log(epoch_idx, "error", "")
        return

    parsed = parse_response(raw_response)

    if parsed is None:
        log.info("No strategy change.")
        append_agent_log(epoch_idx, "no_change", "")
        return

    start_block, end_block, strategy = parsed
    log.info(f"Strategy change: miner 0 -> '{strategy}' "
             f"blocks {start_block}–{end_block}")

    with open(STRATEGY_OUT, "w") as f:
        f.write(f"0, {start_block}, {end_block}, {strategy}\n")

    append_agent_log(epoch_idx, "applied", strategy)


def watch():
    
    log.info(f"Watcher started. Watching for {LOCK_FILE}")
    log.info(f"Epoch rolling: {EPOCH_ROLLING}")
    log.info(f"Block rolling: {BLOCK_ROLLING}")
    log.info(f"Total epochs configured: {TOTAL_EPOCHS}")
    init_agent_log()

    while True:
        if LOCK_FILE.exists():
            log.info("sim_pause.lock detected.")
            try:
                process_epoch()
            except Exception as e:
                log.error(f"Unexpected error: {e}", exc_info=True)
            finally:
                try:
                    LOCK_FILE.unlink(missing_ok=True)
                    log.info("Lock released — sim unblocked.")
                except Exception as e:
                    log.error(f"Failed to remove lock: {e}")

        time.sleep(POLL_INTERVAL)

if __name__ == "__main__":
    watch()