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
SCHEDULE_FILE = CLAUDE_DIR.parent / "example_schedule.txt" 
STRATEGY_OUT  = CLAUDE_DIR / "strategy_update.csv"
AGENT_LOG     = CLAUDE_DIR / "agent_log.csv"
KNOWLEDGE_MD  = CLAUDE_DIR / "knowledge.md"

SIM_OUTPUT_BASE = "agentTest"  # match your sim's output argument
EPOCH_ROLLING = CLAUDE_DIR / f"epoch_rolling.csv"
BLOCK_ROLLING = CLAUDE_DIR / f"block_rolling.csv"

POLL_INTERVAL  = 0.5   # seconds between lock-file polls
API_TIMEOUT    = 30    # seconds before giving up on Claude API call
MODEL          = "claude-sonnet-4-6"
MAX_TOKENS     = 500    # output is one CSV line or nothing

TOTAL_EPOCHS   = 12    # constant — change if sim config changes

VALID_STRATEGIES = {
     "default-selfish", "selfish", 
    "stubborn-trail", "stubborn-fork", "stubborn-lead",
    "stubborn-lead-fork", "stubborn-trail-fork",
    "stubborn-lead-trail", "stubborn-lead-trail-fork",
    "petty", "lazy-fork",  
    "publish-3", "publish-4"
}

INCENTIVE_PATTERN = re.compile(
    r"^incentive-trail-(1|2)(-lead)?(-fork)?-(\d+(\.\d+)?)$"
)

_game_number = 0  # tracks which game we're in

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(CLAUDE_DIR / "watcher.log", mode="a"),
    ]
)
log = logging.getLogger(__name__)



def start_new_game(reason: str = "new game"):
    """Wipe log and seed with this game's initial strategy."""
    global _game_number
    _game_number += 1
    with open(AGENT_LOG, "w") as f:
        f.write("timestamp,game,epoch,decision,strategy,note\n")
    initial_strat = get_scheduled_strategy(0)
    append_agent_log(0, "initial", initial_strat, f"game {_game_number} start ({reason})")
    log.info(f"=== Starting game {_game_number}: initial strategy = {initial_strat} ===")

def append_agent_log(epoch: int, decision: str, strategy: str, note: str = ""):
    with open(AGENT_LOG, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([datetime.utcnow().isoformat(), _game_number, epoch, decision, strategy, note])

def load_schedule(path: Path) -> list[dict]:
    """Parse schedule file into list of {miner_id, start_block, end_block, strategy}."""
    entries = []
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                # Skip config directives
                if line.split(",")[0].strip().upper() in (
                    "FEE_CHANGE", "NOISY_TRANSACTION", "WHALE_ENABLED",
                    "WHALE_PROB", "WHALE_MULTIPLIER"
                ):
                    continue
                parts = [p.strip() for p in line.split(",")]
                if len(parts) < 4:
                    continue
                try:
                    entries.append({
                        "miner_id":    int(parts[0]),
                        "start_block": int(parts[1]),
                        "end_block":   int(parts[2]),
                        "strategy":    parts[3],
                        "gamma":       float(parts[4]) if len(parts) > 4 and parts[4] not in ("-1", "-1.0", "") else -1.0,
                    })

                except (ValueError, IndexError):
                    continue
    except Exception as e:
        log.warning(f"Could not load schedule file: {e}")
    return entries

_schedule = load_schedule(SCHEDULE_FILE)

def get_scheduled_strategy(epoch: int) -> str:
    block = epoch * 2016
    best = INITIAL_STRATEGY
    best_start = -1
    for entry in _schedule:
        if entry["miner_id"] == 0:
            if entry["start_block"] <= block <= entry["end_block"]:
                if entry["start_block"] > best_start:
                    best_start = entry["start_block"]
                    best = entry["strategy"]
    return best

def get_initial_strategy() -> str:
    for entry in _schedule:
        if entry["miner_id"] == 0 and entry["start_block"] == 0:
            return entry["strategy"]
    return "selfish"  

INITIAL_STRATEGY = get_initial_strategy()

def get_last_strategy() -> str:
    return get_current_strategy(0) 

def get_current_strategy(epoch: int = 0) -> str:
    """Return the most recently logged strategy for the current game."""
    if not AGENT_LOG.exists():
        return get_scheduled_strategy(epoch)
    
    last_strategy = None
    try:
        with open(AGENT_LOG) as f:
            for row in csv.DictReader(f):
                s = row.get("strategy", "").strip()
                d = row.get("decision", "").strip()
                if s and d in ("applied", "initial", "schedule"):
                    last_strategy = s  # latest wins
    except Exception:
        pass
    
    return last_strategy if last_strategy else get_scheduled_strategy(epoch)

# utils 
def read_csv_rows(path: Path) -> list[dict]:
    rows = []
    try:
        with open(path) as f:
            lines = [l for l in f if l and not l.startswith("#")]
        reader = csv.DictReader(lines)
        for row in reader:
            cleaned = {}
            for k, v in row.items():
                if k is not None and v is not None:
                    cleaned[k.strip()] = v.strip()
            if cleaned:
                rows.append(cleaned)
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

    m0_alpha = 0.0
    m1_alpha = 0.0       
    gamma_per_miner = {}
    if block_rows:
        last_block = block_rows[-1]
        m0_alpha = safe_float(find_col(last_block, "Miner-0_alpha", "_alpha"))
        m1_alpha = safe_float(find_col(last_block, "Miner-1_alpha", "_alpha"))
        for key in last_block:
            if key.startswith("gamma_miner"):
                miner_id = key.replace("gamma_miner", "")
                val = safe_float(last_block[key], default=-1.0)
                if val >= 0.0:   # -1.0 means attacker, skip
                    gamma_per_miner[miner_id] = val


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
        f"Miner 0 alpha: {m0_alpha:.3f}",
        f"Miner 1 alpha: {m1_alpha:.3f}",
        *(
            [f"Gamma miner {mid}: {g:.3f}  {'(honest ref)' if mid != '0' else '(attacker)'}"
            for mid, g in sorted(gamma_per_miner.items())]
            if gamma_per_miner else ["Gamma: unknown"]
        ), 
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

    strategy_history = []
    try:
        with open(AGENT_LOG) as f:
            for row in csv.DictReader(f):
                s = row.get("strategy", "").strip()
                d = row.get("decision", "").strip()
                e = row.get("epoch", "?")
                g = row.get("game", "?")
                if s and d in ("applied", "initial", "schedule"):
                    tag = {"applied": "AGENT", "initial": "start", 
                        "schedule": "SCHEDULE"}.get(d, d)
                    strategy_history.append(f"  game {g} epoch {e}: {s} ({tag})")
    except Exception:
        pass

    scheduled_now = get_scheduled_strategy(current_epoch)
    current = get_current_strategy(current_epoch)

    lines += [
        f"",
        f"--- Miner 0 Strategy Context (Game {_game_number}) ---",
        f"Current active: {current}",
        f"Schedule says:  {scheduled_now}",
        f"Agent overriding: {current != scheduled_now}",
        f"This game's history:",
    ] + (strategy_history if strategy_history else ["  (no decisions yet)"])

    strategies_tried_this_game = []
    try:
        with open(AGENT_LOG) as f:
            for row in csv.DictReader(f):
                s = row.get("strategy", "").strip()
                if s and s not in strategies_tried_this_game:
                    strategies_tried_this_game.append(s)
    except Exception:
        pass

    lines += [
        f"Strategies tried this game: {', '.join(strategies_tried_this_game)}",
    ]

    current_strategy_age = 0
    last_change_epoch = 0
    try:
        with open(AGENT_LOG) as f:
            rows = list(csv.DictReader(f))
        for row in rows:
            if row.get("decision") in ("applied", "schedule", "initial"):
                last_change_epoch = safe_int(row.get("epoch", "0"))
        current_strategy_age = current_epoch - last_change_epoch
    except Exception:
        pass

    lines += [
        f"Current strategy age: {current_strategy_age} epoch(s)",
    ]
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
            "Decision required: should miner 0 switch strategy?\n\n"
            "Your objective is to MAXIMIZE miner 0's cumulative Revenue Advantage "
            "and MINIMIZE time to profitability (epochs until cumulative RA crosses zero).\n\n"
            "- NEVER switch strategy when: (a) cumulative RA is rising AND (b) remaining epochs * current per-epoch RA > |cumulative deficit|. "
"  Recovery is on track — a single bad epoch is noise. Switching destroys the transition investment already made.\n\n"
"- stubborn-lead-trail and stubborn-lead-trail-fork are HIGH VARIANCE strategies. "
"  Only use them if stubborn-lead has been negative for 3+ consecutive epochs AND alpha clearly exceeds the threshold.\n\n"
            "Do NOT treat the threshold tables as a binary gate. A strategy below the "
            "joint profitability threshold may still be the best available option if "
            "all alternatives are worse. Default-selfish (honest mining) produces "
            "RA = 0 going forward — only choose it if all attack strategies are "
            "structurally expected to produce NEGATIVE RA.\n\n"
            "Constraints/Considerations:\n"
            "- Which strategy combination with miner 1's apparent behavior gives the "
            "highest RA at current alpha and gamma?\n"
            "- Is the current strategy trending toward positive per-epoch RA?\n"
            "- How many epochs remain to recover the cumulative deficit?\n"
            "- Generally, one bad epoch is noise; 2+ bad epochs is signal.\n"
            "If switching: output ONLY: 0, <start_block>, <end_block>, <strategy_name>\n"
            "If no change: output ONLY: NOCHANGE\n"
            "Brief reasoning before the output line is acceptable."
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
    
def has_decisions_beyond_initial() -> bool:
    """Check if log has any non-seed entries (i.e. game is in progress)."""
    if not AGENT_LOG.exists():
        return False
    try:
        with open(AGENT_LOG) as f:
            rows = list(csv.DictReader(f))
        # If we have anything beyond the single 'initial' entry, game is active
        non_initial = [r for r in rows if r.get("decision") != "initial"]
        return len(non_initial) > 0
    except Exception:
        return False

_last_seen_epoch = -1

def process_epoch():
    global _last_seen_epoch
    
    # Peek at epoch from rolling file BEFORE building summary
    epoch_rows = read_csv_rows(EPOCH_ROLLING)
    if not epoch_rows:
        log.info("No epoch data yet — skipping.")
        return
    epoch_idx = safe_int(find_col(epoch_rows[-1], "dap_index"))
    
    is_new_game = (epoch_idx == 0 and 
                   (_last_seen_epoch > 0 or has_decisions_beyond_initial()))
    
    if is_new_game:
        log.info(f"New game detected (last_seen_epoch={_last_seen_epoch}, epoch_idx=0)")
        start_new_game(reason=f"epoch reset {_last_seen_epoch} -> 0")
    
    _last_seen_epoch = epoch_idx
    
    # Log schedule-driven switch if applicable
    scheduled_now = get_scheduled_strategy(epoch_idx)
    scheduled_prev = get_scheduled_strategy(max(0, epoch_idx - 1))
    if epoch_idx > 0 and scheduled_now != scheduled_prev:
        # Only log if agent isn't currently overriding
        current = get_current_strategy(epoch_idx)
        if current == scheduled_prev:  # agent wasn't overriding, schedule actually applies
            append_agent_log(epoch_idx, "schedule", scheduled_now,
                             f"schedule switch at epoch {epoch_idx}")
            log.info(f"Schedule switch logged: {scheduled_prev} -> {scheduled_now}")
    
    state_summary, _ = compute_state_summary()
    log.info(f"Epoch {epoch_idx} state:\n{state_summary}")
    log.info("Calling Claude...")
    
    raw_response = ""
    try:
        raw_response = call_claude(state_summary)
        log.info(f"Claude response: {repr(raw_response)}")
    except anthropic.APITimeoutError:
        log.warning(f"Claude API timed out — no strategy change.")
        return
    except Exception as e:
        log.warning(f"Claude API error: {e} — no strategy change.")
        return

    parsed = parse_response(raw_response)
    if parsed is None:
        log.info("No strategy change.")
        return
    
    start_block, end_block, strategy = parsed
    log.info(f"Strategy change: miner 0 -> '{strategy}' blocks {start_block}-{end_block}")
    
    with open(STRATEGY_OUT, "w") as f:
        f.write(f"0, {start_block}, {end_block}, {strategy}\n")
    
    append_agent_log(epoch_idx, "applied", strategy, f"game {_game_number} agent decision")

def watch():
    log.info(f"Watcher started. Watching for {LOCK_FILE}")
    log.info(f"Epoch rolling: {EPOCH_ROLLING}")
    log.info(f"Block rolling: {BLOCK_ROLLING}")
    log.info(f"Schedule file: {SCHEDULE_FILE}")
    log.info(f"Initial strategy: {INITIAL_STRATEGY}")
    log.info(f"Total epochs configured: {TOTAL_EPOCHS}")
    
    if AGENT_LOG.exists():
        AGENT_LOG.unlink()
        log.info("Cleared stale agent_log.csv from previous run")
    start_new_game(reason="watcher startup")
    
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