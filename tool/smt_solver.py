#!/usr/bin/env python3
# Derives state offsets and transition row values for the 32-bit UTF-8
# validation DFA encoded in utf8_valid.h.
#
# The DFA has 9 states. Each state is assigned a bit offset within a
# 32-bit integer. Each byte class maps to a row value; the 5-bit window
# at offset[src] encodes the target state for that source state:
#
#   next_state = (row >> offset[src]) & 31
#
# S_ERROR is fixed at offset 0. Error transitions contribute 0 to the
# row value and are therefore free. The solver finds the smallest set of
# offsets for the remaining 8 states such that all transition windows fit
# without collision within 32 - 5 = 27 bits.
#
# Minimizing the concatenation of all variables makes the solution unique
# and deterministic across z3 versions.
#
# Ref: https://gist.github.com/dougallj/166e326de6ad4cf2c94be97a204c025f

from z3 import *

# State names and indices.
# Column order in transition rows matches this order.
S_ERROR  = 0
S_ACCEPT = 1
S_TAIL1  = 2
S_E0     = 3
S_TAIL2  = 4
S_ED     = 5
S_F0     = 6
S_F1_F3  = 7
S_F4     = 8

STATE_NAMES = [
    'S_ERROR', 'S_ACCEPT', 'S_TAIL1', 'S_E0', 'S_TAIL2',
    'S_ED', 'S_F0', 'S_F1_F3', 'S_F4',
]
STATE_CNT = len(STATE_NAMES)

ERR = S_ERROR

# Transition table: targets[src] = next state index.
# S_ERROR transitions are free (contribute 0 to the row value).
TRANSITIONS = [
    (ERR, S_ACCEPT, ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # ASCII
    (ERR, S_TAIL1,  ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # C2-DF
    (ERR, S_E0,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # E0
    (ERR, S_TAIL2,  ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # E1-EC, EE-EF
    (ERR, S_ED,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # ED
    (ERR, S_F0,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # F0
    (ERR, S_F1_F3,  ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # F1-F3
    (ERR, S_F4,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # F4
    (ERR, ERR,      S_ACCEPT,ERR,     S_TAIL1, S_TAIL1, ERR,     S_TAIL2, S_TAIL2),  # 80-8F
    (ERR, ERR,      S_ACCEPT,ERR,     S_TAIL1, S_TAIL1, S_TAIL2, S_TAIL2, ERR    ),  # 90-9F
    (ERR, ERR,      S_ACCEPT,S_TAIL1, S_TAIL1, ERR,     S_TAIL2, S_TAIL2, ERR    ),  # A0-BF
    (ERR, ERR,      ERR,     ERR,     ERR,     ERR,     ERR,     ERR,     ERR    ),  # ERROR  C0-C1, F5-FF
]

LABELS = [
    "ASCII",
    "LEAD2",
    "E0",
    "LEAD3",
    "ED",
    "F0",
    "LEAD4",
    "F4",
    "CONT_80_8F",
    "CONT_90_9F",
    "CONT_A0_BF",
    "ERROR",
]

o = Optimize()
offsets    = [BitVec(f"o{i}", 32) for i in range(STATE_CNT)]
row_values = [BitVec(f"r{i}", 32) for i in range(len(TRANSITIONS))]

# S_ERROR fixed at 0; INVALID row is all-error so its value is 0
o.add(offsets[0]     == 0)
o.add(row_values[-1] == 0)

# All offsets distinct and small enough that a 5-bit window fits in 32 bits
for i in range(STATE_CNT):
    o.add(offsets[i] < 32 - 5)
    for j in range(i):
        o.add(offsets[i] != offsets[j])

# For each row and each source state, the 5-bit window at offset[src]
# must equal offset[target]
for row, targets in zip(row_values, TRANSITIONS):
    for src, tgt in enumerate(targets):
        o.add((LShR(row, offsets[src]) & 31) == offsets[tgt])

# Minimize for a unique, deterministic solution
o.minimize(Concat(*offsets, *row_values))

result = o.check()
print(result)
if result != sat:
    raise SystemExit("no solution found")

m = o.model()
offsets_val    = [m[v].as_long() for v in offsets]
row_values_val = [m[v].as_long() for v in row_values]

print("Offsets:")
for name, val in zip(STATE_NAMES, offsets_val):
    print(f"  {name:12s} = {val}")

print("Transitions:")
w = max(len(l) for l in LABELS)
for label, val in zip(LABELS, row_values_val):
    print(f"  {label:{w}s}  =>  {val:#10x},  // {val:032b}")
