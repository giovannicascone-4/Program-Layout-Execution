#include "cpu.h"
#include "control.h"
#include "memory.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s <file.bin> [--dump] [--step] [--trace] [--addr X]\n",
            prog);
}

static int parse_hex_addr(const char *text, uint16_t *out_addr) {
    char *endptr = NULL;
    unsigned long parsed;

    if (text == NULL || out_addr == NULL) {
        return 0;
    }

    errno = 0;
    parsed = strtoul(text, &endptr, 16);
    if (errno != 0 || endptr == text || *endptr != '\0' || parsed > 0xFFFFul) {
        return 0;
    }

    *out_addr = (uint16_t)parsed;
    return 1;
}

int main(int argc, char **argv) {
    const char *bin_path = NULL;
    int do_dump = 0;
    int do_step = 0;
    int do_trace = 0;
    int has_addr = 0;
    uint16_t start_addr = 0u;
    uint16_t end_addr = 0x0100u;
    FILE *fp = NULL;
    long file_size = 0;
    size_t num_words = 0u;
    uint16_t *buffer = NULL;
    size_t words_read = 0u;
    CPU cpu;
    int i;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump") == 0) {
            do_dump = 1;
        } else if (strcmp(argv[i], "--step") == 0) {
            do_step = 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            do_trace = 1;
        } else if (strcmp(argv[i], "--addr") == 0) {
            if (i + 1 >= argc || !parse_hex_addr(argv[i + 1], &start_addr)) {
                print_usage(argv[0]);
                return 1;
            }
            has_addr = 1;
            ++i;
        } else if (argv[i][0] == '-') {
            print_usage(argv[0]);
            return 1;
        } else if (bin_path == NULL) {
            bin_path = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (bin_path == NULL || (do_step && do_trace)) {
        print_usage(argv[0]);
        return 1;
    }

    fp = fopen(bin_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: failed to open '%s'\n", bin_path);
        return 1;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fprintf(stderr, "error: failed to seek '%s'\n", bin_path);
        fclose(fp);
        return 1;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fprintf(stderr, "error: failed to determine size of '%s'\n", bin_path);
        fclose(fp);
        return 1;
    }

    if ((file_size % (long)sizeof(uint16_t)) != 0) {
        fprintf(stderr, "error: binary size must be a multiple of 2 bytes\n");
        fclose(fp);
        return 1;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "error: failed to rewind '%s'\n", bin_path);
        fclose(fp);
        return 1;
    }

    num_words = (size_t)file_size / sizeof(uint16_t);
    if (num_words > MEM_SIZE) {
        fprintf(stderr, "error: program is larger than addressable memory\n");
        fclose(fp);
        return 1;
    }

    if (num_words > 0u) {
        buffer = (uint16_t *)malloc(num_words * sizeof(uint16_t));
        if (buffer == NULL) {
            fprintf(stderr, "error: out of memory allocating program buffer\n");
            fclose(fp);
            return 1;
        }

        words_read = fread(buffer, sizeof(uint16_t), num_words, fp);
        if (words_read != num_words) {
            fprintf(stderr, "error: failed to read binary words from '%s'\n", bin_path);
            free(buffer);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    fp = NULL;

    cpu_init(&cpu);
    cpu_reset(&cpu);

    if (num_words > 0u) {
        mem_load(buffer, (uint16_t)num_words, 0u);
    }

    free(buffer);
    buffer = NULL;

    if (do_step) {
        while (!cpu.halted) {
            cpu_step(&cpu);
            cpu_dump(&cpu);
            (void)getchar();
        }
    } else if (do_trace) {
        while (!cpu.halted) {
            cpu_step(&cpu);
            cpu_dump(&cpu);
        }
    } else {
        cpu_run(&cpu);
    }

    if (do_dump) {
        if (has_addr) {
            uint32_t end_calc = (uint32_t)start_addr + 0x0100u;
            end_addr = (uint16_t)((end_calc > 0xFFFFu) ? 0xFFFFu : end_calc);
        }
        mem_dump(start_addr, end_addr);
    }

    return 0;
}
