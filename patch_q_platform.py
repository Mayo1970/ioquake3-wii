#!/usr/bin/env python3
"""
patch_q_platform.py
Run from the ioquake3-wii/ directory:
    python3 patch_q_platform.py

Adds a GEKKO/Wii block to ../ioq3/code/qcommon/q_platform.h
"""

import sys, os

IOQ3_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ioq3')
QPLAT    = os.path.join(IOQ3_DIR, 'code', 'qcommon', 'q_platform.h')

if not os.path.isfile(QPLAT):
    print(f"ERROR: {QPLAT} not found.")
    print("Make sure ioq3/ is cloned alongside ioquake3-wii/")
    sys.exit(1)

with open(QPLAT, 'r') as f:
    lines = f.readlines()

# Already patched?
if any('GEKKO' in l for l in lines):
    print("Already patched — nothing to do.")
    sys.exit(0)

# Print lines 340-360 so the user can see context
print("=== q_platform.h lines 340-365 ===")
for i, l in enumerate(lines[339:364], start=340):
    print(f"{i:4d}: {l}", end='')
print("===================================\n")

# Find the #else that precedes "Operating system not supported"
insert_before = None
for i, l in enumerate(lines):
    if 'Operating system not supported' in l:
        # Walk back to find the #else
        for j in range(i, max(i-10, 0), -1):
            if lines[j].strip().startswith('#else'):
                insert_before = j
                break
        break

if insert_before is None:
    print("ERROR: Could not find insertion point.")
    print("Please add the GEKKO block manually — see README.")
    sys.exit(1)

WII_BLOCK = """\

//===========================================================================
// Nintendo Wii / WiiU (Gekko/Broadway PowerPC) — ioquake3-wii port
//===========================================================================
#elif defined(GEKKO)

#define OS_STRING  "wii"
#define ARCH_STRING "ppc"
#define PATH_SEP   '/'
#define DLL_EXT    ".so"

#undef  Q3_LITTLE_ENDIAN
#define Q3_BIG_ENDIAN

#ifndef ID_INLINE
#  define ID_INLINE static __inline__
#endif

"""

lines.insert(insert_before, WII_BLOCK)

# Write back
with open(QPLAT, 'w') as f:
    f.writelines(lines)

print(f"Patched: {QPLAT}")
print(f"Inserted Wii block before line {insert_before+1}")
print("\nNow run:  make dol")
