#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "h78.h"
#include "lz78.h"

/*
 * Type definitions.
 */
typedef unsigned short lz78_id_t;
typedef unsigned char lz78_symbol_t;
typedef struct lz78_trie_node lz78_trie_node_t;
typedef struct lz78_dict_entry lz78_dict_entry_t;

/* using a trie avoids repeated comparisons over the entire dictionary
 * when encoding */
struct lz78_trie_node {
    lz78_id_t id;
    lz78_symbol_t symbol;
    struct lz78_trie_node *left;
    struct lz78_trie_node *right;
};

/* decoding builds each dictionary entry as linked list from the last
 * character to the first, reduces memory usage */
struct lz78_dict_entry {
    struct lz78_dict_entry *prev;
    lz78_symbol_t symbol;
};

/*
 * Function prototypes.
 */
static lz78_trie_node_t *make_node(lz78_symbol_t symbol, lz78_id_t id);
static void write_dict_entry(FILE *fp, lz78_dict_entry_t *entry);

/*
 * Global variables.
 */
static unsigned short magic = 0x4838;   /* "H8" magic number for LZ78 */

/*
 * Code
 */
int lz78_encode(const char *infile, const char *outfile)
{
    lz78_trie_node_t *root = NULL, **p;
    lz78_id_t id = 0;
    lz78_symbol_t symbol;
    char *filename;
    size_t namelen;
    FILE *fin, *fout;
    lz78_id_t next = 1;
    int rc;

    fin = fopen(infile, "rb");

    if (!fin) {
        perror("fopen");
        return 1;
    }

    fout = fopen(outfile, "wb");

    if (!fout) {
        perror("fopen");
        return 1;
    }

    /* write header */
    filename = basename(strdup(infile));
    namelen = strlen(filename);

    fwrite(&magic, sizeof(magic), 1, fout);
    fwrite(&namelen, sizeof(namelen), 1, fout);
    fwrite(filename, namelen + 1, 1, fout);

    p = &root;

    while (!feof(fin)) {
        rc = fread(&symbol, sizeof(symbol), 1, fin);

        if (!rc) {
            /* EOF, if we were traversing the tree for a prefix, output
             * the prefix id */
            if (id) fwrite(&id, sizeof(id), 1, fout);
            break;
        }

        while (*p && (*p)->symbol != symbol) {
            p = &(*p)->left;
        }

        if (!*p) {
            if (next) {
                *p = make_node(symbol, next);
                next++;
            }

            fwrite(&id, sizeof(id), 1, fout);
            fwrite(&symbol, sizeof(symbol), 1, fout);
            p = &root;
            id = 0;
        } else {
            id = (*p)->id;
            p = &(*p)->right;
        }
    }

    fclose(fout);

    fclose(fin);

    return 0;
}

int lz78_decode(const char *infile, char *outfile)
{
    lz78_dict_entry_t dict[1 << bitsof(lz78_id_t)];
    lz78_symbol_t symbol;
    unsigned short m;
    char out[256];
    size_t namelen;
    FILE *fin, *fout;
    lz78_id_t id, next = 1;

    fin = fopen(infile, "rb");

    if (!fin) {
        perror("fopen");
        return 1;
    }

    fread(&m, sizeof(m), 1, fin);

    if (m != magic) {
        fprintf(stderr, "Invalid file type.\n");
        return 1;
    }

    fread(&namelen, sizeof(namelen), 1, fin);
    fread(out, sizeof(char), namelen + 1, fin);

    if (outfile) strcpy(outfile, out);

    fout = fopen(out, "wb");

    if (!fout) {
        perror("fopen");
        return 1;
    }

    while (!feof(fin)) {
        if (!fread(&id, sizeof(id), 1, fin)) {
            break;
        }

        if (id) {
            write_dict_entry(fout, &dict[id]);
        }

        if (fread(&symbol, sizeof(symbol), 1, fin)) {
            fwrite(&symbol, sizeof(symbol), 1, fout);

            if (next) {
                dict[next].prev = (id ? &dict[id] : NULL);
                dict[next].symbol = symbol;
                next++;
            }
        }
    }

    fclose(fout);

    fclose(fin);

    return 0;
}

static lz78_trie_node_t *make_node(lz78_symbol_t symbol, lz78_id_t id)
{
    lz78_trie_node_t *node;

    node = (lz78_trie_node_t *) malloc(sizeof(lz78_trie_node_t));

    node->id = id;
    node->symbol = symbol;
    node->left = NULL;
    node->right = NULL;

    return node;
}

static void write_dict_entry(FILE *fp, lz78_dict_entry_t *entry)
{
    if (entry->prev) {
        write_dict_entry(fp, entry->prev);
    }

    fwrite(&entry->symbol, sizeof(entry->symbol), 1, fp);
}

