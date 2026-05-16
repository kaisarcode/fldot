# fldot — Flow to DOT Graph Converter

## Overview

fldot is a command-line tool that converts `.flow` documents into DOT graph syntax for rendering with Graphviz. It parses flow definitions containing nodes, links, function references, and file references, and produces a directed graph that visualizes the execution topology. The generated DOT output includes styled nodes, typed edges, metadata-driven labels, and a dark-orange KaisarCode product theme.

fldot is the visualization counterpart to `flow.c` in the kaisarcode ecosystem: `flow.c` defines and interprets runtime flow logic, while fldot reads the same `.flow` format and generates a static architecture diagram. Together they form a documentation-through-code pipeline where flow files serve as both runtime configuration and diagram source.

## Architecture

A single-pass pipeline reads `.flow` key-value records into a dynamic array, then a second pass discovers graph topology by scanning for `flow.id`, `flow.link`, `flow.meta.title`, `node.*`, `func.*`, and `file.*` references. For each discovered element, the generator emits DOT node/edge declarations with theme-driven styling. Computed edges (heredoc `link` values) trigger a heuristic `printf` target scanner that extracts linked node names from shell script bodies, falling back to a diamond-shaped computed node when no targets are found.

## Directory Layout

| Path | Contents |
|------|----------|
| `src/fldot.c` | Single translation unit: CLI, parser, graph builder, DOT emitter (568 lines) |
| `src/theme.h` | Visualization theme: 48 macros for colors, shapes, fonts, pen widths (68 lines) |
| `README.md` | User-facing documentation with examples and build instructions |
| `test.sh` | Functional test suite: 3 test cases covering basic exec, explicit output, metadata titles |
| `Makefile` | Cross-compilation Makefile: 17 targets across Linux/Windows/Android |
| `CMakeLists.txt` | CMake build config: C11, O3, Wall/Wextra/Werror, optional march=native |
| `etc/example.flow` | Example flow document illustrating all edge types and metadata |
| `etc/example.dot` | Expected DOT output for the example flow |
| `etc/example.png` | Rendered Graphviz output of the example |
| `bin/` | Build artifacts in `bin/{arch}/{platform}/fldot` |
| `.build/` | CMake intermediate build directory |
| `.kcsignore` | KCS ignore list |
| `LICENSE` | GPL v3 |

## Data Model

### Internal Structures

| Symbol | Type | Role |
|--------|------|------|
| `kc_flow_record` | `struct { char *key; char *value; int heredoc; }` | Single key-value pair from a `.flow` file; `heredoc` flag distinguishes literal vs. computed values |
| `kc_flow_records` | `struct { kc_flow_record *items; size_t count; size_t cap; }` | Dynamic array of parsed records; capacity doubles on overflow (initial cap 64) |
| `kc_flow_strings` | `struct { char **items; size_t count; size_t cap; }` | Dynamic array of unique C strings (no duplicates); used for discovered node/function/file names |

### Hard Limits

| Limit | Value | Symbol |
|-------|-------|--------|
| Input line length | 8191 bytes | `char line[8192]` in `kc_fldot_read_records`, `kc_read_heredoc` |
| Heredoc initial buffer | 4096 bytes | `cap = 4096` in `kc_read_heredoc`, doubles on overflow |
| Records initial capacity | 64 | `cap = 64` in `kc_records_add`, doubles on overflow |
| Strings initial capacity | 64 | `cap = 64` in `kc_strings_add_unique`, doubles on overflow |
| Node name buffer | 1023 chars | `char b[1024]` in DOT output for `node:`, `func:`, `file:` IDs |
| File label buffer | 2047 chars | `char l[2048]` for `file:` node labels (`"child flow\n%s"`) |
| Meta key max | `strlen(prefix) + strlen(ref) + 6 + strlen(field)` + 1 | Dynamic allocation in `kc_meta_key` |
| Version string | `"0.1.0"` | `KC_FLDOT_VERSION` macro |

## Input Format (.flow)

fldot reads the `.flow` document format as defined by `flow.c` in the kaisarcode ecosystem. Records are line-oriented with the following syntax:

- **key=value** — Literal key-value pair. Both key and value are trimmed of leading/trailing whitespace. Keys may contain dots for namespacing (e.g. `node.router.meta.title`).
- **key=<<MARKER** — Heredoc value. All subsequent lines up to a line containing only `MARKER` (trimmed) form the value. Used for multi-line content such as shell scripts.
- **# comment** — Lines starting with `#` are skipped. Empty lines are also skipped.

Example:
```flow
flow.id=my_pipeline
flow.link=<<EOF
printf "stage_a"
EOF
node.stage_a.exec=run.sh
```

## DOT Generation Pipeline

### 1. Record Parsing

`kc_fldot_read_records` (`src/fldot.c:301`) reads lines from a `FILE*` stream, trimming whitespace and skipping comments/empties. For each `key=value` line, it checks whether the value starts with `<<`. If so, `kc_read_heredoc` (`src/fldot.c:273`) accumulates lines into a growing buffer until the terminator marker is found. The record is stored with `heredoc=1`. Plain values are stored with `heredoc=0`. Returns 0 on success, -1 on failure.

### 2. Graph Topology Discovery

During the first scan of records in `kc_fldot_generate_dot` (`src/fldot.c:329`), each key is classified via `kc_field_after` which extracts the reference name and field after `node.` or `func.` prefixes:

| Record Key Pattern | Action |
|--------------------|--------|
| `flow.id=...` | Sets `flow_id` (defaults to `"flow"`) |
| `flow.link=<plain>` | Adds value as discovered node |
| `node.<ref>.<field>` | Adds `<ref>` to nodes set |
| `node.<ref>.link=<plain>` | Adds value (link target) to nodes set |
| `node.<ref>.use=<value>` | Adds value to nodes set |
| `node.<ref>.file=<value>` | Adds value to files set |
| `func.<ref>.<field>` | Adds `<ref>` to funcs set |

`kc_meta_value` resolves metadata from records like `node.foo.meta.title` or `func.bar.meta.title` for display labels. `flow.meta.title` becomes the graph label; falls back to `flow_id`.

### 3. Edge Typing & Analysis

| Link Source | Edge Type | Line Style | DOT Attributes |
|-------------|-----------|------------|----------------|
| `flow.link` (plain) | entry | solid | `"flow:entry" -> "node:<value>", style=solid, color=#ff4500, penwidth=1.8` |
| `flow.link` (heredoc) | computed | dashed | `"flow:entry" -> "node:<target>"` per `printf` target, style=dashed, color=#ff8a00, penwidth=1.35 |
| `node.<ref>.link` (plain) | runtime | solid | `"node:<ref>" -> "node:<value>", style=solid, color=#f5f5f5, penwidth=1.8` |
| `node.<ref>.link` (heredoc) | computed | dashed | Either edges to discovered `printf` targets (dashed, #ff8a00, 1.35) or falls back to a `"computed:<ref>"` diamond node |
| `node.<ref>.use` | use | dashed | `"node:<ref>" -> "node:<value>", style=dashed, color=#bdbdbd, penwidth=1.35` |
| `node.<ref>.file` | file | dotted | `"node:<ref>" -> "file:<value>", style=dotted, color=#ff6a00, penwidth=1.8` |

The heuristic `kc_scan_printf_targets` (`src/fldot.c:236`) scans heredoc bodies for `printf` calls (plain or quoted), collects non-whitespace tokens, and deduplicates them via `kc_strings_add_unique`. This enables discovery of computed link targets without evaluating the shell script. The scan handles both `printf "word"` and `printf word` forms, filtering out the bare format specifier `s`.

When a node's heredoc link yields zero `printf` targets (e.g. the script is too complex or uses indirect references), fldot creates a `computed:<ref>` placeholder node with diamond shape as a visual indicator of an unresolved computed edge.

### 4. DOT Output

`kc_fldot_generate_dot` emits a DOT `digraph` with this structure:

1. **Header**: `digraph "<flow_id>" { ... }` with graph-level attributes from theme macros (rankdir=LR, splines=ortho, dark background, label from `flow.meta.title`).
2. **Default node/edge attributes**: Node shape, fill, border, font, margin; edge arrowsize and penwidth — all from theme macros.
3. **Entry node**: `"flow:entry"` with oval shape, orange border, penwidth 2.2.
4. **Node declarations**: `"node:<ref>"` with label from `node.<ref>.meta.title` (falls back to `<ref>`), inheriting default box shape/theme.
5. **Function declarations**: `"func:<ref>"` with component shape, orange border, penwidth 1.8, label from `func.<ref>.meta.title`.
6. **File declarations**: `"file:<ref>"` with folder shape, orange border, penwidth 1.8, label `"child flow\n<ref>"`.
7. **Edge declarations**: Sorted by edge type (entry, runtime, computed, use, file) as described in section 3.
8. **Footer**: Closing brace, then memory cleanup of all dynamic arrays.

String values are DOT-quoted via `kc_dot_quote` which escapes `"`, `\`, newline (`\n`), and strips `\r`.

## Theme System (theme.h)

All visual properties are controlled by macros in `src/theme.h`. The default theme is a dark/orange KaisarCode product theme with 48 macros:

### Colors (18 macros)

| Macro | Value | Usage |
|-------|-------|-------|
| `KCV_BG` | `#1f1f1f` | Graph background |
| `KCV_GRAPH_COLOR` | `#111111` | Reserved |
| `KCV_GRAPH_TEXT` | `#ffffff` | Graph title text |
| `KCV_NODE_FILL` | `#111111` | Default node fill |
| `KCV_NODE_BORDER` | `#2c2c2c` | Default node border |
| `KCV_NODE_TEXT` | `#ffffff` | Default node text |
| `KCV_ENTRY_FILL` | `#111111` | Entry node fill |
| `KCV_ENTRY_BORDER` | `#ff4500` | Entry node border (orange-red) |
| `KCV_FUNC_FILL` | `#1c1c1c` | Function node fill |
| `KCV_FUNC_BORDER` | `#ff4500` | Function node border |
| `KCV_FILE_FILL` | `#242424` | File node fill |
| `KCV_FILE_BORDER` | `#ff6a00` | File node border |
| `KCV_COMPUTED_FILL` | `#242424` | Computed placeholder fill |
| `KCV_COMPUTED_BORDER` | `#ff8a3d` | Computed placeholder border |
| `KCV_ENTRY_EDGE` | `#ff4500` | Entry edge color |
| `KCV_RUNTIME_EDGE` | `#f5f5f5` | Runtime (plain link) edge color |
| `KCV_COMPUTED_EDGE` | `#ff8a00` | Computed (heredoc link) edge color |
| `KCV_USE_EDGE` | `#bdbdbd` | `node.X.use` edge color |
| `KCV_FILE_EDGE` | `#ff6a00` | `node.X.file` edge color |

### Graph Layout (6 macros)

| Macro | Value | DOT Attribute |
|-------|-------|---------------|
| `KCV_GRAPH_RANKDIR` | `LR` | rankdir |
| `KCV_GRAPH_SPLINES` | `ortho` | splines |
| `KCV_GRAPH_NODESEP` | `1.25` | nodesep |
| `KCV_GRAPH_RANKSEP` | `1.75` | ranksep |
| `KCV_GRAPH_PAD` | `0.6` | pad |
| `KCV_GRAPH_FONTSIZE` | `24` | Graph label font size |

### Node Shapes & Style (8 macros)

| Macro | Value | DOT Attribute |
|-------|-------|---------------|
| `KCV_NODE_SHAPE` | `box` | Default node shape |
| `KCV_NODE_STYLE` | `rounded,filled` | Default node style |
| `KCV_NODE_MARGIN` | `0.16,0.10` | Default node margin |
| `KCV_NODE_FONTSIZE` | `12` | Default node font size |
| `KCV_ENTRY_SHAPE` | `oval` | Entry node shape |
| `KCV_FUNC_SHAPE` | `component` | Function node shape |
| `KCV_FILE_SHAPE` | `folder` | File node shape |
| `KCV_COMPUTED_SHAPE` | `diamond` | Computed placeholder shape |

### Edge Styles (5 macros)

| Macro | Value | Edge Type |
|-------|-------|-----------|
| `KCV_ENTRY_EDGE_STYLE` | `solid` | `flow.link` plain |
| `KCV_RUNTIME_EDGE_STYLE` | `solid` | `node.X.link` plain |
| `KCV_COMPUTED_EDGE_STYLE` | `dashed` | Any heredoc link edge |
| `KCV_USE_EDGE_STYLE` | `dashed` | `node.X.use` edge |
| `KCV_FILE_EDGE_STYLE` | `dotted` | `node.X.file` edge |

### Pen Widths & Arrow Size (11 macros)

| Macro | Value | Applied To |
|-------|-------|------------|
| `KCV_EDGE_ARROWSIZE` | `0.65` | All edges (default) |
| `KCV_EDGE_PENWIDTH` | `1.2` | All edges (default) |
| `KCV_ENTRY_PENWIDTH` | `2.2` | Entry node border |
| `KCV_FUNC_PENWIDTH` | `1.8` | Function node border |
| `KCV_FILE_PENWIDTH` | `1.8` | File node border |
| `KCV_COMPUTED_PENWIDTH` | `1.8` | Computed node border |
| `KCV_ENTRY_EDGE_PENWIDTH` | `1.8` | Entry edges |
| `KCV_RUNTIME_EDGE_PENWIDTH` | `1.8` | Runtime edges |
| `KCV_COMPUTED_EDGE_PENWIDTH` | `1.35` | Computed edges |
| `KCV_USE_EDGE_PENWIDTH` | `1.35` | Use edges |
| `KCV_FILE_EDGE_PENWIDTH` | `1.8` | File edges |

## CLI

| Flag | Argument | Required | Description |
|------|----------|----------|-------------|
| `-i`, `--input` | `<file>` | Yes | Input `.flow` file path |
| `-o`, `--output` | `<file>` | No | Output `.dot` file path (default: input path with `.dot` extension) |
| `-h`, `--help` | — | No | Print usage and exit 0 |
| `-v`, `--version` | — | No | Print version (`fldot 0.1.0`) and exit 0 |

If no `-o` is given and the input ends with `.flow`, the extension is replaced with `.dot`. Otherwise `.dot` is appended. Output written to stdout: no, always to file.

### Exit Codes

| Code | Condition |
|------|-----------|
| 0 | Success |
| 1 | Allocation failure, missing argument, unknown option, missing `-i`, file open failure, parse failure, DOT generation failure |

## Build

| Target | Description |
|--------|-------------|
| `make` (default) | Build for native host architecture/platform |
| `make all` | Build all 17 architecture/platform combinations |
| `make test` | Run `sh test.sh` |
| `make clean` | Remove `.build/` directory |
| `<arch>/<platform>` | Cross-compile for specific target (e.g. `x86_64/linux`, `aarch64/android`) |

Native target detection uses `uname -m` and `uname -s` to select the appropriate toolchain. Supported architectures: x86_64, i686, aarch64, armv7, armv7hf, riscv64, powerpc64le, mips, mipsel, mips64el, s390x, loongarch64. Supported platforms: linux, windows, android.

CMake builds with `-O3 -Wall -Wextra -Werror` on non-MSVC, `-O2 /W3 /WX` on MSVC. Optional `-march=native` via `-DBIN_NATIVE=ON`. C11 standard. Links `pthread` and `m` on UNIX non-Android.

## Error Handling

| Scenario | Behavior | Code Location |
|----------|----------|---------------|
| Unknown CLI flag | `fprintf(stderr, "fldot: unknown option '%s'\n", argv[i]); return 1` | `main:507` |
| Missing flag argument | `fprintf(stderr, "fldot: missing argument for %s\n", argv[i]); return 1` | `main:496,502` |
| No input file | `fprintf(stderr, "fldot: missing required input file (-i)\n"); return 1` | `main:514` |
| Cannot open input | `perror(input_path); return 1` | `main:536` |
| Cannot open output | `perror(output_path); return 1` | `main:551` |
| malloc/realloc fails | `exit(1)` (no diagnostic) | `kc_xmalloc:52`, `kc_records_add:116`, `kc_strings_add_unique:151`, `kc_read_heredoc:286`, `kc_meta_key:187` |
| Parse failure | `return -1` from `kc_fldot_read_records` → `main:542` | Propagated to `return 1` |

All error paths free any already-allocated memory (derived output path, records, file handles) before returning.

## Constraints

- **Single translation unit**: All logic is in `src/fldot.c` with only `src/theme.h` as a dependency. No library component (no `libfldot.c` or `fldot.h`).
- **No stdin mode**: Input must always be a file specified via `-i`. Output is always written to a file, never to stdout.
- **No JSON/YAML**: Only supports the `.flow` key-value line format.
- **Static buffers**: Input lines capped at 8191 bytes. Longer lines are silently truncated by `fgets`.
- **Heredoc scanning is heuristic**: `kc_scan_printf_targets` only finds `printf` calls, not `echo`, variable indirection, or function calls that might reference other nodes. Complex computed links may produce a diamond placeholder instead of resolved edges.
- **No cycle detection**: The graph may contain cycles if flow documents define circular links.
- **No subgraph clustering**: All nodes sit at the top level; no subgraphs for namespaces or scopes.
- **Metadata is optional**: Missing `meta.title` values fall back to the raw reference name without warning.
- **Output overwrites**: No check for existing `.dot` file; `fopen(..., "w")` truncates silently.
- **POSIX-only**: Requires `_POSIX_C_SOURCE 200809L` for `strdup`/`strndup` (via custom wrappers) and `getline` is not used (custom line reading with `fgets`).
- **No input validation beyond parse errors**: Semantic errors (nonexistent node references, duplicate definitions) are silently ignored.
- **Toolchain dependency for cross-compiles**: Android builds require the NDK at `$ANDROID_HOME`; Linux cross builds require the corresponding `*-linux-gnu-gcc` toolchains.
