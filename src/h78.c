#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "h78.h"
#include "huffman.h"
#include "lz78.h"

/*
 * Function prototypes.
 */
void usage(void);
int init(const char *options);
int encode(const char *filename);
int decode(const char *filename);

/*
 * Global variables.
 */
static char *usage_msg =
    "Usage: %s c[l] FILE\n"
    "  or:  %s x[l] FILE\n"
    "\n"
    "Options displayed in square brackets are optional.\n"
    "  c    compress a file using adaptive Huffman coding, output name\n"
    "       will be the original name with .csz suffix\n"
    "  l    enable LZ78 layer\n"
    "  x    decompress an adaptive Huffman encoded file\n"
    "\n"
    "Author: Cesar L. B. Silveira <cesarbs@gmail.com>\n";

char *prog_name;    /* how the program was called */
int prog_opts = 0;  /* program options: verbose, use LZ78 */

/*
 * Code.
 */
int main(int argc, char **argv)
{
    char option;
    int rc = 0;

    prog_name = argv[0];

    if (argc != 3) {
        usage();
        exit(1);
    }

    if (init(argv[1])) {
        usage();
        exit(1);
    }

    if (prog_opts & COMPRESS) {
        rc = encode(argv[2]);
    } else {
        rc = decode(argv[2]);
    }

    return rc;
}

void usage(void) {
    fprintf(stderr, usage_msg, prog_name, prog_name);
}

int init(const char *options)
{
    const char *p;
    int rc = 0;

    huffman_init();

    /* parse command line options */
    for (p = options; *p; p++) {
        switch (*p) {
        case 'c':
            prog_opts |= COMPRESS;
            break;
        case 'l':
            prog_opts |= LZ78;
            break;
        case 'x':
            prog_opts |= DECOMPRESS;
            break;
        default:
            fprintf(stderr, "error: invalid option: %c\n", *p);
            rc = 1;
        }
    }

    if (!rc && !(prog_opts & (COMPRESS | DECOMPRESS))) {
        fprintf(stderr, "error: at least c or x must be specified.\n");
        rc = 1;
    }

    if (!rc && (prog_opts & COMPRESS) && (prog_opts & DECOMPRESS)) {
        fprintf(stderr, "error: conflicting options.\n");
        rc = 1;
    }

    return rc;
}

int encode(const char *infile) {
    char *filename, lz78_outfile[256], huffman_outfile[256];
    struct stat s;
    off_t orig_size, compressed_size;
    int rc = 0;

    filename = basename(strdup(infile));

    strcpy(lz78_outfile, ".");
    strcat(lz78_outfile, filename);
    strcat(lz78_outfile, ".lz78");

    strcpy(huffman_outfile, filename);
    strcat(huffman_outfile, ".csz");

    if (prog_opts & LZ78) {
        rc = lz78_encode(infile, lz78_outfile);

        if (!rc) {
            rc = huffman_encode(lz78_outfile, huffman_outfile);
        }

        unlink(lz78_outfile);
    } else {
        rc = huffman_encode(infile, huffman_outfile);
    }

    if (!rc) {
        stat(infile, &s);
        orig_size = s.st_size;
        stat(huffman_outfile, &s);
        compressed_size = s.st_size;

        printf("Original file size: %d bytes\n", orig_size);
        printf("Compressed file size: %d bytes\n", compressed_size);
        printf("Compression ratio: %f\n",
            (float) compressed_size / orig_size);
    }

    return rc;
}

int decode(const char *filename) {
    char huffman_outfile[256];
    int rc = 0;

    rc = huffman_decode(filename, huffman_outfile);

    if (!rc && (prog_opts & LZ78)) {
        rc = lz78_decode(huffman_outfile, NULL);
        unlink(huffman_outfile);
    }

    return rc;
}
