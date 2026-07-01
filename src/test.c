/**
 * test.c - fldot CLI contract tests.
 * Summary: Tests the compiled fldot executable through real CLI invocations.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: https://www.gnu.org/licenses/gpl-3.0.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define popen _popen
#define pclose _pclose
#define getpid _getpid
#else
#include <unistd.h>
#endif

#ifndef KC_FLDOT_TEST_CLI
#define KC_FLDOT_TEST_CLI "./bin/x86_64/linux/fldot"
#endif

/**
 * Verifies one boolean condition.
 * @param name Check description.
 * @param condition Non-zero when the check passed.
 * @return 0 on success, 1 on failure.
 */
static int expect_true(const char *name, int condition) {
    if (!condition) {
        printf("\033[31m[FAIL]\033[0m %s\n", name);
        return 1;
    }
    printf("\033[32m[PASS]\033[0m %s\n", name);
    return 0;
}

/**
 * Drains and closes one pipe, returning the process exit code.
 * @param fp Open pipe from popen.
 * @return Exit code from the closed process.
 */
static int drain_and_close(FILE *fp) {
    char buf[256];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
    }
    return pclose(fp);
}

/**
 * Runs the CLI with given arguments and captures its exit code.
 * @param args Arguments string (may be empty).
 * @return Exit code from the CLI.
 */
static int run_cli(const char *args) {
    char cmd[4096];
    FILE *fp;
    int n;

    n = snprintf(cmd, sizeof(cmd), "\"%s\" %s",
        KC_FLDOT_TEST_CLI, args ? args : "");
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        return -1;
    }

    fp = popen(cmd, "r");
    if (!fp) return -1;
    return drain_and_close(fp);
}

/**
 * Builds a unique temporary file path in the current directory.
 * @param out Output buffer.
 * @param out_size Buffer size.
 * @param prefix File name prefix.
 * @return Nothing.
 */
static void temp_path(char *out, size_t out_size, const char *prefix) {
    snprintf(out, out_size, "%s_%d.tmp", prefix, (int)getpid());
}

/**
 * Tests --help exits successfully.
 * @return 0 on success, 1 on failure.
 */
static int case_help(void) {
    return expect_true("--help exits 0", run_cli("--help") == 0);
}

/**
 * Tests --version exits successfully.
 * @return 0 on success, 1 on failure.
 */
static int case_version(void) {
    return expect_true("--version exits 0", run_cli("--version") == 0);
}

/**
 * Tests basic DOT generation from a flow file.
 * @return 0 on success, 1 on failure.
 */
static int case_basic_output(void) {
    char args[1024];
    char flow_path[256];
    char dot_path[512];
    FILE *fp;
    int found;
    int ch;

    temp_path(flow_path, sizeof(flow_path), "fldot_basic");
    fp = fopen(flow_path, "w");
    if (!fp) return 1;
    fprintf(fp, "flow.id=test\nnode.a.link=b\nnode.b.exec=ls\n");
    fclose(fp);

    snprintf(dot_path, sizeof(dot_path), "%s.dot", flow_path);

    snprintf(args, sizeof(args), "-i %s", flow_path);

    if (run_cli(args) != 0) {
        remove(flow_path);
        remove(dot_path);
        return expect_true("basic execution exits 0", 0);
    }

    fp = fopen(dot_path, "r");
    if (!fp) {
        remove(flow_path);
        remove(dot_path);
        return expect_true("derived .dot file created", 0);
    }

    found = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 'd') {
            char buf[8];
            size_t i;
            buf[0] = 'd';
            for (i = 1; i < sizeof(buf); i++) {
                ch = fgetc(fp);
                if (ch == EOF) break;
                buf[i] = (char)ch;
            }
            if (i == sizeof(buf) && memcmp(buf, "digraph", 7) == 0) {
                found = 1;
                break;
            }
        }
    }
    fclose(fp);

    remove(flow_path);
    remove(dot_path);

    return expect_true("DOT output contains 'digraph'", found);
}

/**
 * Tests explicit -o output path.
 * @return 0 on success, 1 on failure.
 */
static int case_explicit_output(void) {
    char flow_path[256];
    char dot_path[512];
    char args[1024];
    FILE *fp;

    temp_path(flow_path, sizeof(flow_path), "fldot_explicit");
    fp = fopen(flow_path, "w");
    if (!fp) return 1;
    fprintf(fp, "flow.id=test\n");
    fclose(fp);

    temp_path(dot_path, sizeof(dot_path), "fldot_explicit_out");

    snprintf(args, sizeof(args), "-i %s -o %s",
        flow_path, dot_path);

    if (run_cli(args) != 0) {
        remove(flow_path);
        remove(dot_path);
        return expect_true("explicit output exits 0", 0);
    }

    fp = fopen(dot_path, "r");
    remove(flow_path);
    remove(dot_path);
    return expect_true("explicit .dot file created", fp != NULL);
}

/**
 * Tests metadata titles in generated DOT labels.
 * @return 0 on success, 1 on failure.
 */
static int case_meta_titles(void) {
    char flow_path[256];
    char dot_path[512];
    char args[1024];
    FILE *fp;
    int found_graph = 0;
    int found_node = 0;
    int found_func = 0;
    char buf[512];

    temp_path(flow_path, sizeof(flow_path), "fldot_meta");
    fp = fopen(flow_path, "w");
    if (!fp) return 1;
    fprintf(fp,
        "flow.id=test\n"
        "flow.meta.title=Visible Graph\n"
        "node.router.meta.title=Request Router\n"
        "node.router.link=done\n"
        "node.done.exec=cat\n"
        "func.wrap.meta.title=Response Wrapper\n"
        "func.wrap.exec=cat\n");
    fclose(fp);

    temp_path(dot_path, sizeof(dot_path), "fldot_meta_out");

    snprintf(args, sizeof(args), "-i %s -o %s",
        flow_path, dot_path);

    if (run_cli(args) != 0) {
        remove(flow_path);
        remove(dot_path);
        return expect_true("metadata title execution exits 0", 0);
    }

    fp = fopen(dot_path, "r");
    if (!fp) {
        remove(flow_path);
        remove(dot_path);
        return expect_true("metadata .dot file created", 0);
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (strstr(buf, "label=\"Visible Graph\"")) found_graph = 1;
        if (strstr(buf, "\"node:router\" [label=\"Request Router\"]")) found_node = 1;
        if (strstr(buf, "\"func:wrap\" [label=\"Response Wrapper\"")) found_func = 1;
    }
    fclose(fp);

    remove(flow_path);
    remove(dot_path);

    return expect_true("flow metadata title label", found_graph)
        + expect_true("node metadata title label", found_node)
        + expect_true("function metadata title label", found_func);
}

/**
 * Runs one fldot CLI test case.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 or 2 on failure.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "test case: expected one argument, got %d\n", argc - 1);
        return 2;
    }
    if (strcmp(argv[1], "help") == 0) return case_help();
    if (strcmp(argv[1], "version") == 0) return case_version();
    if (strcmp(argv[1], "basic-output") == 0) return case_basic_output();
    if (strcmp(argv[1], "explicit-output") == 0) return case_explicit_output();
    if (strcmp(argv[1], "meta-titles") == 0) return case_meta_titles();
    fprintf(stderr, "unknown test case: %s\n", argv[1]);
    return 2;
}
