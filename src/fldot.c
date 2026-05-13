/**
 * fldot.c - flow visualization CLI
 * Summary: Command line interface for converting flow documents to SVG.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: https://www.gnu.org/licenses/gpl-3.0.html
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "theme.h"
#define KC_FLDOT_VERSION "0.1.0"

/**
 * @struct kc_flow_record
 * @brief Represents a single key-value pair from a flow file.
 */
typedef struct {
    char *key;
    char *value;
    int heredoc;
} kc_flow_record;

/**
 * @struct kc_flow_records
 * @brief Dynamic array of flow records.
 */
typedef struct {
    kc_flow_record *items;
    size_t count;
    size_t cap;
} kc_flow_records;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} kc_flow_strings;

/**
 * @brief Allocate memory or exit on failure.
 * @param size Size in bytes.
 * @return Pointer to allocated memory.
 */
static void *kc_xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) exit(1);
    return p;
}

/**
 * @brief Duplicate a string or exit on failure.
 * @param s String to duplicate.
 * @return Duplicated string.
 */
static char *kc_xstrdup(const char *s) {
    char *p = (char *)kc_xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/**
 * @brief Duplicate a string with length or exit on failure.
 * @param s String to duplicate.
 * @param n Number of characters.
 * @return Duplicated string.
 */
static char *kc_xstrndup(const char *s, size_t n) {
    char *p = (char *)kc_xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/**
 * @brief Trim whitespace from both ends of a string.
 * @param s String to trim.
 * @return Pointer to the trimmed string within the original.
 */
static char *kc_trim(char *s) {
    char *e;
    while (*s && isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

/**
 * @brief Check if a string starts with a prefix.
 * @param s String to check.
 * @param prefix Prefix to look for.
 * @return 1 if it starts with prefix, 0 otherwise.
 */
static int kc_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * @brief Add a record to the dynamic array.
 * @param r Records array.
 * @param key Key string.
 * @param value Value string.
 * @param heredoc 1 if it's a heredoc, 0 otherwise.
 * @return None.
 */
static void kc_records_add(kc_flow_records *r, const char *key, const char *value, int heredoc) {
    if (r->count == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 64;
        r->items = (kc_flow_record *)realloc(r->items, r->cap * sizeof(*r->items));
        if (!r->items) exit(1);
    }
    r->items[r->count].key = kc_xstrdup(key);
    r->items[r->count].value = kc_xstrdup(value);
    r->items[r->count].heredoc = heredoc;
    r->count++;
}

/**
 * @brief Add a unique string to the dynamic array.
 * @param s Strings array.
 * @param v Value to add.
 * @return None.
 */
static void kc_strings_add_unique(kc_flow_strings *s, const char *v) {
    if (!v || !*v) return;
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->items[i], v) == 0) return;
    }
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 64;
        s->items = (char **)realloc(s->items, s->cap * sizeof(*s->items));
        if (!s->items) exit(1);
    }
    s->items[s->count++] = kc_xstrdup(v);
}

/**
 * @brief Write a quoted string to a stream for DOT.
 * @param out Output stream.
 * @param s String to quote.
 * @return None.
 */
static void kc_dot_quote(FILE *out, const char *s) {
    const unsigned char *p;
    fputc('"', out);
    for (p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc(*p, out);
        } else if (*p == '\n') {
            fputs("\\n", out);
        } else if (*p != '\r') {
            fputc(*p, out);
        }
    }
    fputc('"', out);
}

/**
 * @brief Extract field after "node." or "func." prefix.
 * @param key Full key.
 * @param prefix Prefix ("node." or "func.").
 * @param ref_out Pointer to string to receive the reference name.
 * @return Pointer to string to receive the field name (caller must free).
 */
static char *kc_field_after(const char *key, const char *prefix, char **ref_out) {
    const char *p;
    const char *dot;
    if (!kc_starts_with(key, prefix)) return NULL;
    p = key + strlen(prefix);
    dot = strchr(p, '.');
    if (!dot || dot == p || !dot[1]) return NULL;
    *ref_out = kc_xstrndup(p, (size_t)(dot - p));
    return kc_xstrdup(dot + 1);
}

/**
 * @brief Scan a script for printf targets (heuristic).
 * @param script Shell script body.
 * @param targets Strings array to collect targets.
 * @return None.
 */
static void kc_scan_printf_targets(const char *script, kc_flow_strings *targets) {
    const char *p = script;
    while ((p = strstr(p, "printf")) != NULL) {
        const char *q = p + 6;
        if (p > script && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) {
            p++;
            continue;
        }
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q == '\'' || *q == '"') {
            char quote = *q++;
            const char *start = q;
            while (*q && *q != quote) q++;
            if (q > start) {
                char *target = kc_xstrndup(start, (size_t)(q - start));
                if (!strchr(target, ' ') && *target) kc_strings_add_unique(targets, target);
                free(target);
            }
        } else {
            const char *start = q;
            while (*q && (isalnum((unsigned char)*q) || strchr("_-.<>", *q))) q++;
            if (q > start) {
                char *target = kc_xstrndup(start, (size_t)(q - start));
                if (*target && strcmp(target, "s") != 0) kc_strings_add_unique(targets, target);
                free(target);
            }
        }
        p = q;
    }
}

/**
 * @brief Read a heredoc body from a file stream.
 * @param fp Input stream.
 * @param marker Heredoc terminator.
 * @return Heredoc body (caller must free).
 */
static char *kc_read_heredoc(FILE *fp, const char *marker) {
    char *buf = NULL;
    size_t size = 0, cap = 0;
    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        char tmp[8192];
        snprintf(tmp, sizeof(tmp), "%s", line);
        if (strcmp(kc_trim(tmp), marker) == 0) return buf ? buf : kc_xstrdup("");
        size_t n = strlen(line);
        if (size + n + 1 > cap) {
            cap = cap ? cap * 2 : 4096;
            while (cap < size + n + 1) cap *= 2;
            buf = (char *)realloc(buf, cap);
            if (!buf) exit(1);
        }
        memcpy(buf + size, line, n);
        size += n;
        buf[size] = '\0';
    }
    return buf ? buf : kc_xstrdup("");
}

/**
 * Reads flow records from a file stream.
 * @param fp Input stream.
 * @param r Records array to populate.
 * @return 0 on success, -1 on failure.
 */
static int kc_fldot_read_records(FILE *fp, kc_flow_records *r) {
    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        char *s = kc_trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = kc_trim(s);
        char *value = kc_trim(eq + 1);
        if (kc_starts_with(value, "<<")) {
            char *marker = kc_trim(value + 2);
            char *body = kc_read_heredoc(fp, marker);
            kc_records_add(r, key, body, 1);
            free(body);
        } else {
            kc_records_add(r, key, value, 0);
        }
    }
    return 0;
}

/**
 * Generates DOT representation of the flow and writes to out.
 * @param r Records array.
 * @param out Output stream for DOT.
 * @return 0 on success, -1 on failure.
 */
static int kc_fldot_generate_dot(const kc_flow_records *r, FILE *out) {
    kc_flow_strings nodes = {0}, funcs = {0}, files = {0};
    const char *flow_id = "flow";
    for (size_t i = 0; i < r->count; i++) {
        char *ref = NULL, *field;
        if ((field = kc_field_after(r->items[i].key, "node.", &ref))) {
            kc_strings_add_unique(&nodes, ref);
            if (strcmp(field, "link") == 0 && !r->items[i].heredoc) kc_strings_add_unique(&nodes, r->items[i].value);
            if (strcmp(field, "use") == 0) kc_strings_add_unique(&nodes, r->items[i].value);
            if (strcmp(field, "file") == 0) kc_strings_add_unique(&files, r->items[i].value);
            free(field); free(ref);
        } else if ((field = kc_field_after(r->items[i].key, "func.", &ref))) {
            kc_strings_add_unique(&funcs, ref);
            free(field); free(ref);
        } else if (strcmp(r->items[i].key, "flow.id") == 0) {
            flow_id = r->items[i].value;
        } else if (strcmp(r->items[i].key, "flow.link") == 0 && !r->items[i].heredoc) {
            kc_strings_add_unique(&nodes, r->items[i].value);
        }
    }

    fprintf(out, "digraph "); kc_dot_quote(out, flow_id); fputs(" {\n", out);
    fprintf(out, "  graph [rankdir=LR, bgcolor=\"" KCV_BG "\", fontname=\"" KCV_FONT "\", fontcolor=\"" KCV_GRAPH_TEXT "\", label=\"%s\", labelloc=t, fontsize=24, pad=0.6, nodesep=1.25, ranksep=1.75, splines=ortho];\n", flow_id);
    fprintf(out, "  node [shape=box, style=\"rounded,filled\", fillcolor=\"" KCV_NODE_FILL "\", color=\"" KCV_NODE_BORDER "\", fontname=\"" KCV_FONT "\", fontcolor=\"" KCV_NODE_TEXT "\", fontsize=12, margin=\"0.16,0.10\"];\n");
    fprintf(out, "  edge [arrowsize=0.65, penwidth=1.2];\n");
    kc_dot_quote(out, "flow:entry"); fputs(" [label=\"flow.link\\nentry\", shape=oval, fillcolor=\"" KCV_ENTRY_FILL "\", color=\"" KCV_ENTRY_BORDER "\", penwidth=2.2];\n", out);

    for (size_t i = 0; i < nodes.count; i++) {
        char b[1024]; snprintf(b, sizeof(b), "node:%s", nodes.items[i]);
        kc_dot_quote(out, b); fprintf(out, " [label="); kc_dot_quote(out, nodes.items[i]); fputs("];\n", out);
    }
    for (size_t i = 0; i < funcs.count; i++) {
        char b[1024]; snprintf(b, sizeof(b), "func:%s", funcs.items[i]);
        char l[1024]; snprintf(l, sizeof(l), "func.%s", funcs.items[i]);
        kc_dot_quote(out, b); fprintf(out, " [label="); kc_dot_quote(out, l); fputs(", shape=component, fillcolor=\"" KCV_FUNC_FILL "\", color=\"" KCV_FUNC_BORDER "\", penwidth=1.8];\n", out);
    }
    for (size_t i = 0; i < files.count; i++) {
        char b[1024]; snprintf(b, sizeof(b), "file:%s", files.items[i]);
        char l[2048]; snprintf(l, sizeof(l), "child flow\\n%s", files.items[i]);
        kc_dot_quote(out, b); fprintf(out, " [label="); kc_dot_quote(out, l); fputs(", shape=folder, fillcolor=\"" KCV_FILE_FILL "\", color=\"" KCV_FILE_BORDER "\", penwidth=1.8];\n", out);
    }

    for (size_t i = 0; i < r->count; i++) {
        kc_flow_record rec = r->items[i];
        if (strcmp(rec.key, "flow.link") == 0) {
            if (rec.heredoc) {
                kc_flow_strings t = {0}; kc_scan_printf_targets(rec.value, &t);
                for (size_t j = 0; j < t.count; j++) {
                    char to[1024]; snprintf(to, sizeof(to), "node:%s", t.items[j]);
                    kc_dot_quote(out, "flow:entry"); fputs(" -> ", out); kc_dot_quote(out, to);
                    fputs(" [style=dashed, color=\"" KCV_COMPUTED_EDGE "\", penwidth=1.35];\n", out);
                }
            } else {
                char to[1024]; snprintf(to, sizeof(to), "node:%s", rec.value);
                kc_dot_quote(out, "flow:entry"); fputs(" -> ", out); kc_dot_quote(out, to);
                fputs(" [color=\"" KCV_ENTRY_EDGE "\", penwidth=1.8];\n", out);
            }
        } else {
            char *ref = NULL, *field = kc_field_after(rec.key, "node.", &ref);
            if (!field) continue;
            char from[1024]; snprintf(from, sizeof(from), "node:%s", ref);
            if (strcmp(field, "link") == 0) {
                if (rec.heredoc) {
                    kc_flow_strings t = {0}; kc_scan_printf_targets(rec.value, &t);
                    if (t.count == 0) {
                        char to[1024]; snprintf(to, sizeof(to), "computed:%s", ref);
                        char l[1024]; snprintf(l, sizeof(l), "computed link\\n%s", ref);
                        kc_dot_quote(out, to); fprintf(out, " [label="); kc_dot_quote(out, l); fputs(", shape=diamond, fillcolor=\"" KCV_COMPUTED_FILL "\", color=\"" KCV_COMPUTED_BORDER "\", penwidth=1.8];\n", out);
                        kc_dot_quote(out, from); fputs(" -> ", out); kc_dot_quote(out, to); fputs(" [style=dashed, color=\"" KCV_COMPUTED_EDGE "\", penwidth=1.35];\n", out);
                    } else {
                        for (size_t j = 0; j < t.count; j++) {
                            char to[1024]; snprintf(to, sizeof(to), "node:%s", t.items[j]);
                            kc_dot_quote(out, from); fputs(" -> ", out); kc_dot_quote(out, to); fputs(" [style=dashed, color=\"" KCV_COMPUTED_EDGE "\", penwidth=1.35];\n", out);
                        }
                    }
                } else {
                    char to[1024]; snprintf(to, sizeof(to), "node:%s", rec.value);
                    kc_dot_quote(out, from); fputs(" -> ", out); kc_dot_quote(out, to); fputs(" [color=\"" KCV_RUNTIME_EDGE "\", penwidth=1.8];\n", out);
                }
            } else if (strcmp(field, "use") == 0) {
                char to[1024]; snprintf(to, sizeof(to), "node:%s", rec.value);
                kc_dot_quote(out, from); fputs(" -> ", out); kc_dot_quote(out, to); fputs(" [style=dashed, color=\"" KCV_USE_EDGE "\", penwidth=1.35];\n", out);
            } else if (strcmp(field, "file") == 0) {
                char to[1024]; snprintf(to, sizeof(to), "file:%s", rec.value);
                kc_dot_quote(out, from); fputs(" -> ", out); kc_dot_quote(out, to); fputs(" [style=dotted, color=\"" KCV_FILE_EDGE "\", penwidth=1.8];\n", out);
            }
            free(field); free(ref);
        }
    }
    fputs("}\n", out);
    for (size_t i = 0; i < nodes.count; i++) { free(nodes.items[i]); }
    free(nodes.items);
    for (size_t i = 0; i < funcs.count; i++) { free(funcs.items[i]); }
    free(funcs.items);
    for (size_t i = 0; i < files.count; i++) { free(files.items[i]); }
    free(files.items);
    return 0;
}

/**
 * Frees memory allocated for flow records.
 * @param r Records array.
 * @return None.
 */
static void kc_fldot_free_records(kc_flow_records *r) {
    for (size_t i = 0; i < r->count; i++) {
        free(r->items[i].key);
        free(r->items[i].value);
    }
    free(r->items);
    r->items = NULL;
    r->count = r->cap = 0;
}

/**
 * Print command usage information.
 * @param name Program executable name.
 * @return None.
 */
static void kc_fldot_print_help(const char *name) {
    printf("Usage: %s -i [input.flow] [-o output.dot]\n", name);
    printf("\n");
    printf("Options:\n");
    printf("    -i, --input <file>  Input .flow file\n");
    printf("    -o, --output <file> Output .dot file (default: same path as input with .dot extension)\n");
    printf("    -h, --help          Show this help message\n");
    printf("    -v, --version       Show version\n");
}

/**
 * Print command version information.
 * @return None.
 */
static void kc_fldot_print_version(void) {
    printf("fldot %s\n", KC_FLDOT_VERSION);
}

/**
 * Main entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process status code.
 */
int main(int argc, char **argv) {
    const char *input_path = NULL;
    const char *output_path = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            kc_fldot_print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            kc_fldot_print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "fldot: missing argument for %s\n", argv[i]);
                return 1;
            }
            input_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "fldot: missing argument for %s\n", argv[i]);
                return 1;
            }
            output_path = argv[++i];
        } else {
            fprintf(stderr, "fldot: unknown option '%s'\n", argv[i]);
            return 1;
        }
        i++;
    }

    if (!input_path) {
        fprintf(stderr, "fldot: missing required input file (-i)\n");
        return 1;
    }

    char *derived_out = NULL;
    if (!output_path) {
        size_t len = strlen(input_path);
        if (len >= 5 && strcmp(input_path + len - 5, ".flow") == 0) {
            derived_out = kc_xstrndup(input_path, len - 5);
            char *tmp = (char *)kc_xmalloc(len + 1);
            sprintf(tmp, "%s.dot", derived_out);
            free(derived_out);
            derived_out = tmp;
        } else {
            derived_out = (char *)kc_xmalloc(len + 5);
            sprintf(derived_out, "%s.dot", input_path);
        }
        output_path = derived_out;
    }

    FILE *fp = fopen(input_path, "r");
    if (!fp) {
        perror(input_path);
        if (derived_out) free(derived_out);
        return 1;
    }

    kc_flow_records records = {0};
    if (kc_fldot_read_records(fp, &records) != 0) {
        fclose(fp);
        if (derived_out) free(derived_out);
        return 1;
    }
    fclose(fp);

    FILE *out_fp = fopen(output_path, "w");
    if (!out_fp) {
        perror(output_path);
        kc_fldot_free_records(&records);
        if (derived_out) free(derived_out);
        return 1;
    }

    if (kc_fldot_generate_dot(&records, out_fp) != 0) {
        fclose(out_fp);
        kc_fldot_free_records(&records);
        if (derived_out) free(derived_out);
        return 1;
    }

    fclose(out_fp);
    kc_fldot_free_records(&records);
    if (derived_out) free(derived_out);
    return 0;
}
