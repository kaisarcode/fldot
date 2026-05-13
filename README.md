# fldot - Flow to DOT Converter

`fldot` is a command-line tool that converts `.flow` documents into DOT graph syntax. It parses node definitions, inheritance, and link structures to generate a visual representation of the execution flow that can be rendered using Graphviz or other DOT-compatible tools.

![Example Flow](etc/example.png)

---

## CLI

Example CLI interface provided by the application.

### Examples

Generate DOT in the same directory (creates `example.dot`):

```bash
./bin/x86_64/linux/fldot -i example.flow
```

Generate DOT in a specific location:

```bash
./bin/x86_64/linux/fldot -i example.flow -o build/output.dot
```

Render the generated DOT file to PNG using Graphviz:

```bash
dot -Tpng example.dot -o example.png
```

---

### Parameters

| Command/Flag | Description |
| :--- | :--- |
| `-i`, `--input <file>` | Input `.flow` file (required) |
| `-o`, `--output <file>` | Output `.dot` file (optional, defaults to input path with `.dot` extension) |
| `-h`, `--help` | Show help and usage |
| `-v`, `--version` | Show version |

---

## Flow Metadata

Flow documents may attach display metadata under the `meta` namespace:

```flow
flow.meta.title=Website Runtime
flow.meta.summary=Registers the listener and dispatches parsed requests.

node.router.meta.title=Request Router
node.router.meta.summary=Extracts request.path from parsed HTTP fields.
```

`fldot` builds graph structure from execution fields such as `flow.link`,
`node.*.link`, `node.*.use`, and `node.*.file`. It uses `flow.meta.title` as
the visible graph label, and `node.*.meta.title` or `func.*.meta.title` as
visible element labels when present. Other metadata fields remain valid Flow
data and are safe to keep in input documents; `*.meta.summary` can later map to
DOT tooltip or graph-comment attributes.

---

## Theming

The visual appearance of the generated DOT graphs is fully customizable through the `src/theme.h` C header file. `fldot` is built with a sleek, dark KaisarCode product theme by default. 

To change background colors, border colors, typography, node shapes, or edge styles, simply modify the macro definitions in `src/theme.h` and rebuild the project:

```c
#define KCV_BG                    "#1f1f1f"
#define KCV_NODE_FILL             "#111111"
#define KCV_NODE_SHAPE            "box"
#define KCV_ENTRY_EDGE_STYLE      "solid"
#define KCV_FILE_SHAPE            "folder"
```

---

## Build

Compiled artifacts are generated under `bin/{arch}/{platform}/` for the host architecture running the build.

```bash
make clean && make
```

## Multiarch Builds

The project is prepared to build artifacts for multiple architectures under `bin/{arch}/{platform}/`. A plain `make` builds only the current host architecture, while the targets below build the full matrix or a specific target.

```bash
make all
make x86_64/linux
make x86_64/windows
make i686/linux
make i686/windows
make aarch64/linux
make aarch64/android
make armv7/linux
make armv7/android
make armv7hf/linux
make riscv64/linux
make powerpc64le/linux
make mips/linux
make mipsel/linux
make mips64el/linux
make s390x/linux
make loongarch64/linux
```

---

**Author:** KaisarCode

**Email:** <kaisar@kaisarcode.com>

**Website:** [https://kaisarcode.com](https://kaisarcode.com)

**License:** [GNU GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.html)

© 2026 KaisarCode
