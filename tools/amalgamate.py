#!/usr/bin/env python3
"""Generate the single-header Bedrock distribution under dist/.

Bedrock is authored as a modular tree (include/bedrock/<module>/<file>.h plus
src/**/*.c). This script folds that tree into two release artifacts:

    dist/bedrock.h  - declarations always; implementation under
                      #ifdef BEDROCK_IMPLEMENTATION
    dist/bedrock.c  - a two-line translation unit that defines
                      BEDROCK_IMPLEMENTATION and includes "bedrock.h"

The layout is discovered, never hardcoded: the declaration set comes from
recursively expanding include/bedrock.h, and the implementation set comes from
globbing src/**/*.c. Adding a module requires no change here.

Core invariant (H1): every feature-test macro (_GNU_SOURCE, _POSIX_C_SOURCE,
WIN32_LEAN_AND_MEAN, _WIN32_WINNT) is stripped from the folded bodies and
re-emitted once at the very top of the implementation section, before any
#include, because a feature-test macro must precede the first system header in
its translation unit to take effect.

The output is deterministic: sources are folded in a stable, path-sorted order
and the script performs no time- or environment-dependent formatting, so
`make check-dist` can diff a fresh run against the committed artifacts.
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = REPO_ROOT / "include"
SRC_DIR = REPO_ROOT / "src"
DIST_DIR = REPO_ROOT / "dist"

MASTER_HEADER = INCLUDE_DIR / "bedrock.h"

# Angle-bracket include of a Bedrock header, e.g. #include <bedrock/mem/alloc.h>.
BEDROCK_ANGLE_INCLUDE = re.compile(r'^\s*#\s*include\s*<(bedrock/[^>]+)>\s*$')
# Quoted include, used inside src/ for the sibling internal.h header.
QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"\s*$')

# Feature-test macros that must be hoisted above every #include (H1). Matches
# both the bare `#define X` / `#define X value` form and tolerates surrounding
# `#ifndef X` / `#endif` guards, which are dropped when the macro is stripped.
#
# _DARWIN_C_SOURCE is emitted even though no source defines it: folding every
# translation unit into one means _POSIX_C_SOURCE (which time_unix.c sets) now
# also applies to the Darwin backends, and strict _POSIX_C_SOURCE hides the BSD
# extensions those backends need (MAP_ANONYMOUS, pthread_threadid_np). Defining
# _DARWIN_C_SOURCE re-enables them and is inert on non-Apple platforms. This is
# an amalgamation-only hazard the per-file build never hits.
FEATURE_MACRO_NAMES = (
    "_GNU_SOURCE",
    "_POSIX_C_SOURCE",
    "_DARWIN_C_SOURCE",
    "WIN32_LEAN_AND_MEAN",
    "_WIN32_WINNT",
)
FEATURE_DEFINE = re.compile(
    r'^\s*#\s*define\s+(' + "|".join(FEATURE_MACRO_NAMES) + r')\b\s*(.*?)\s*$'
)
FEATURE_IFNDEF = re.compile(
    r'^\s*#\s*ifndef\s+(' + "|".join(FEATURE_MACRO_NAMES) + r')\s*$'
)

BANNER = """\
/*
 * Bedrock - single-header distribution
 *
 * GENERATED FILE - DO NOT EDIT.
 * Regenerate with `make dist` (tools/amalgamate.py) from the modular tree
 * under include/ and src/. Edits here are overwritten and will fail
 * `make check-dist`.
 *
 * Usage:
 *   #include "bedrock.h"                       // declarations
 *   #define BEDROCK_IMPLEMENTATION             // in exactly one .c
 *   #include "bedrock.h"
 * or compile the companion dist/bedrock.c, which does the above for you.
 */
"""


def read_lines(path):
    return path.read_text(encoding="utf-8").splitlines()


def strip_feature_macros(lines):
    """Remove feature-test macro definitions (and their #ifndef/#endif guards).

    Returns the body with those lines removed. Handles the canonical idiom

        #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
        #endif

    as well as a bare `#define _GNU_SOURCE`. The macros are re-emitted once at
    the top of the implementation section by the caller.
    """
    out = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        ifndef = FEATURE_IFNDEF.match(line)
        if ifndef and i + 2 < n:
            define = FEATURE_DEFINE.match(lines[i + 1])
            endif = re.match(r'^\s*#\s*endif\b.*$', lines[i + 2])
            if define and define.group(1) == ifndef.group(1) and endif:
                i += 3
                continue
        if FEATURE_DEFINE.match(line):
            i += 1
            continue
        out.append(line)
        i += 1
    return out


def expand_header(path, visited, out):
    """Recursively inline a Bedrock header once, in include order.

    Bedrock angle includes are expanded (each path at most once, tracked in
    `visited`). System includes are emitted verbatim and rely on their own
    include guards. Inner `#ifndef BEDROCK_*_H` guards are left in place: they
    are idempotent and make a second textual inclusion a harmless no-op.
    """
    rel = path.relative_to(INCLUDE_DIR).as_posix()
    if rel in visited:
        return
    visited.add(rel)

    out.append("/* ==== %s ==== */" % rel)
    for line in read_lines(path):
        m = BEDROCK_ANGLE_INCLUDE.match(line)
        if m:
            target = INCLUDE_DIR / m.group(1)
            if target.exists():
                expand_header(target, visited, out)
                continue
            # Unknown bedrock header: keep the include so the failure is loud.
        out.append(line)
    out.append("")


def fold_source(path, out):
    """Append one src/*.c body, dropping includes already satisfied by the fold.

    Bedrock angle includes (declarations) are removed because the whole public
    API precedes this section. Quoted includes (the sibling internal.h) are
    resolved and inlined once via the shared visited set. System includes and
    feature macros are handled by the caller (macros are pre-stripped and
    hoisted); everything else is emitted verbatim.
    """
    rel = path.relative_to(REPO_ROOT).as_posix()
    out.append("/* ==== %s ==== */" % rel)
    lines = strip_feature_macros(read_lines(path))
    for line in lines:
        if BEDROCK_ANGLE_INCLUDE.match(line):
            continue
        if QUOTED_INCLUDE.match(line):
            # Sibling headers (internal.h) are inlined separately, once.
            continue
        out.append(line)
    out.append("")


def inline_src_header(path, visited, out):
    """Inline a src-local header (internal.h) once, stripping bedrock includes.

    Its Bedrock angle includes are already satisfied by the declaration
    section; system includes stay. Feature macros are pre-stripped.
    """
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel in visited:
        return
    visited.add(rel)
    out.append("/* ==== %s ==== */" % rel)
    for line in strip_feature_macros(read_lines(path)):
        if BEDROCK_ANGLE_INCLUDE.match(line):
            continue
        out.append(line)
    out.append("")


def build_header():
    parts = []
    parts.append(BANNER)
    parts.append("#ifndef BEDROCK_SINGLE_H")
    parts.append("#define BEDROCK_SINGLE_H")
    parts.append("")

    # Declaration section: expand the master umbrella. The visited set is shared
    # with the implementation fold so a header is never inlined twice.
    parts.append("/* ======================================================")
    parts.append("   Declarations (always visible)")
    parts.append("   ====================================================== */")
    parts.append("")
    visited = set()
    header_body = []
    expand_header(MASTER_HEADER, visited, header_body)
    parts.extend(header_body)

    # Implementation section, opt-in.
    parts.append("#ifdef BEDROCK_IMPLEMENTATION")
    parts.append("")
    parts.append("/* Feature-test macros hoisted above every #include (H1). */")
    for name in FEATURE_MACRO_NAMES:
        value = FEATURE_MACRO_VALUES[name]
        parts.append("#ifndef %s" % name)
        parts.append("#define %s%s" % (name, (" " + value) if value else ""))
        parts.append("#endif")
    parts.append("")

    # Inline the single src-local header (internal.h) before the bodies that
    # need it. Discovered by globbing, not hardcoded.
    src_headers = sorted(SRC_DIR.rglob("*.h"), key=lambda p: p.as_posix())
    for hdr in src_headers:
        inline_src_header(hdr, visited, parts)

    # Fold every implementation file in a stable, path-sorted order.
    src_files = sorted(SRC_DIR.rglob("*.c"), key=lambda p: p.as_posix())
    for src in src_files:
        fold_source(src, parts)

    parts.append("#endif /* BEDROCK_IMPLEMENTATION */")
    parts.append("")
    parts.append("#endif /* BEDROCK_SINGLE_H */")
    parts.append("")
    return "\n".join(parts)


def build_source():
    return (
        "/*\n"
        " * Bedrock - single translation unit for the amalgamated distribution.\n"
        " *\n"
        " * GENERATED FILE - DO NOT EDIT. Regenerate with `make dist`.\n"
        " */\n"
        "#define BEDROCK_IMPLEMENTATION\n"
        '#include "bedrock.h"\n'
    )


# Canonical values for the hoisted feature macros, harvested from the tree so
# the emitted definitions match what the sources expect (e.g. the exact
# _POSIX_C_SOURCE level and _WIN32_WINNT target).
FEATURE_MACRO_VALUES = {}


def harvest_feature_macro_values():
    values = {name: "" for name in FEATURE_MACRO_NAMES}
    for src in sorted(SRC_DIR.rglob("*.c")) + sorted(SRC_DIR.rglob("*.h")):
        for line in read_lines(src):
            m = FEATURE_DEFINE.match(line)
            if m:
                name, value = m.group(1), m.group(2)
                if value and not values[name]:
                    values[name] = value
    return values


def main():
    if not MASTER_HEADER.exists():
        sys.stderr.write("error: %s not found\n" % MASTER_HEADER)
        return 1

    global FEATURE_MACRO_VALUES
    FEATURE_MACRO_VALUES = harvest_feature_macro_values()

    DIST_DIR.mkdir(exist_ok=True)
    (DIST_DIR / "bedrock.h").write_text(build_header(), encoding="utf-8")
    (DIST_DIR / "bedrock.c").write_text(build_source(), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
