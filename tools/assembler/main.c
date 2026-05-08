/*
 * main.c --- Command-line driver.
 *
 * Usage:
 *   assembler <src.asm>
 *   assembler <src.asm> -o out.bin
 *   assembler <src.asm> --listing
 *   assembler <src.asm> --symbols
 *
 * Flags may appear in any order. If -o is omitted, the output file
 * name is derived from the input by replacing its extension with .bin
 * (or appending .bin if there was none).
 */
#include "assembler.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f, const char *prog) {
    fprintf(f,
        "usage: %s <src.asm> [-o out.bin] [--listing] [--symbols]\n"
        "\n"
        "  -o <file>   override output filename\n"
        "  --listing   print address + hex + source line for each instruction\n"
        "  --symbols   print the final symbol table\n"
        "  -h, --help  show this help and exit\n",
        prog);
}

/* Return a malloc'd output path derived from the input path. */
static char *derive_output_path(const char *in) {
    size_t n = strlen(in);
    char *out = malloc(n + 5); /* ".bin" + NUL */
    if (!out) return NULL;

    const char *dot = strrchr(in, '.');
    const char *slash = strrchr(in, '/');
    if (dot && (!slash || dot > slash)) {
        size_t base = (size_t)(dot - in);
        memcpy(out, in, base);
        memcpy(out + base, ".bin", 5);
    } else {
        memcpy(out, in, n);
        memcpy(out + n, ".bin", 5);
    }
    return out;
}

int main(int argc, char **argv) {
    const char *src_path = NULL;
    const char *out_path = NULL;
    AsmOptions opt = { .listing = false, .dump_symbols = false };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) { usage(stderr, argv[0]); return 2; }
            out_path = argv[++i];
            continue;
        }
        if (strcmp(a, "--listing") == 0) { opt.listing = true; continue; }
        if (strcmp(a, "--symbols") == 0) { opt.dump_symbols = true; continue; }
        if (a[0] == '-') {
            fprintf(stderr, "unknown option: %s\n", a);
            usage(stderr, argv[0]);
            return 2;
        }
        if (!src_path) { src_path = a; continue; }

        fprintf(stderr, "unexpected positional argument: %s\n", a);
        usage(stderr, argv[0]);
        return 2;
    }

    if (!src_path) {
        usage(stderr, argv[0]);
        return 2;
    }

    char *derived = NULL;
    if (!out_path) {
        derived = derive_output_path(src_path);
        if (!derived) {
            fprintf(stderr, "out-of-memory deriving output path\n");
            return 1;
        }
        out_path = derived;
    }

    AsmResult res = {0};
    AsmError rc = assemble_file(src_path, out_path, &opt, &res);

    if (rc == ERR_OK && res.n_errors == 0) {
        printf("assembled %s -> %s  (origin 0x%04X, %u words)\n",
               src_path, out_path, res.origin, (unsigned)res.n_words);
        free(derived);
        return 0;
    }

    fprintf(stderr, "assembly failed with %d error(s)\n", res.n_errors);
    free(derived);
    return 1;
}
