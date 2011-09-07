#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include "huffman.h"
#include "h78.h"

/*
 * Macro definitions.
 */
#define NSYMBOLS 256 /* 256 possible bytes */
#define is_l_child(node) (node->parent && node->parent->l_child == node)
#define is_leaf(node) (!node->l_child && !node->r_child)

/*
 * Type definitions.
 */
typedef unsigned short huffman_id_t;
typedef unsigned int huffman_weight_t;
typedef struct huffman_node huffman_node_t;

struct huffman_node {
    huffman_id_t id;
    huffman_node_t *parent;
    huffman_node_t *l_child;
    huffman_node_t *r_child;
    huffman_weight_t weight;
    unsigned char symbol;
};

/*
 * Function prototypes.
 */
static huffman_node_t *make_node(huffman_node_t *parent);
static huffman_node_t *nyt_spawn(unsigned char symbol);
static void update(huffman_node_t *node);
static huffman_node_t *group_highest_id(huffman_weight_t weight,
                                        huffman_node_t *node);
static void swap_nodes(huffman_node_t *node1, huffman_node_t *node2);
static int get_node_path(huffman_node_t *node, unsigned int *path);
static void write_bits(FILE *fp, unsigned int bits, unsigned short n);
static void write_bit(FILE *fp, unsigned char bit);
static int read_bit(FILE *fp);

/*
 * Global variables.
 */
huffman_node_t *leaf[NSYMBOLS]; /* direct access to leaf (symbol) nodes */
huffman_node_t *nyt;            /* direct access to NYT node */
huffman_node_t *root = NULL;    /* Huffman tree root */
static unsigned short magic = 0x4855;  /* 'HU' magic number for Huffman */

/* Extern global variables. */
extern int prog_opts;           /* program options: verbose, use LZ78 */

/*
 * Code.
 */
void huffman_init(void)
{
    int i;

    /* initialize Huffman tree with NYT at root */
    root = nyt = make_node(NULL);
    root->id = NSYMBOLS;

    for (i = 0; i < NSYMBOLS; i++) {
        leaf[i] = NULL;
    }
}

int huffman_encode(const char *infile, const char *outfile)
{
    unsigned char symbol;
    unsigned short nbits;
    unsigned int path;
    unsigned long totalbits = 0;
    huffman_node_t *node, *old_nyt;
    char *filename;
    size_t namelen;
    FILE *fin, *fout;

    /* open files */
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
    fwrite(&totalbits, sizeof(totalbits), 1, fout);
    fwrite(&namelen, sizeof(namelen), 1, fout);
    fwrite(filename, sizeof(char), namelen + 1, fout);

    /* start encoding */
    while (!feof(fin)) {
        if (!fread(&symbol, sizeof(symbol), 1, fin)) {
            break;
        }

        if (!leaf[symbol]) {
            path = symbol;
            path <<= (bitsof(path) - bitsof(symbol));
            nbits = bitsof(symbol);

            nbits += get_node_path(nyt, &path);

            totalbits += nbits;

            /* write nyt path and symbol bits to file */
            write_bits(fout, path, nbits);

            /* update tree */
            node = nyt_spawn(symbol);
            update(node->parent);
        } else {
            nbits = get_node_path(leaf[symbol], &path);

            totalbits += nbits;

            /* write symbol path bits to file */
            write_bits(fout, path, nbits);

            /* update tree */
            update(leaf[symbol]);
        }
    }

    /* flush remaining bits */
    write_bits(fout, 0, 8);

    /* update total bits header field */
    fseek(fout, sizeof(magic), SEEK_SET);
    fwrite(&totalbits, sizeof(totalbits), 1, fout);

    fclose(fout);
    fclose(fin);

    return 0;
}

int huffman_decode(const char *infile, char *outfile)
{
    huffman_node_t *node;
    unsigned char symbol;
    unsigned short m;
    unsigned long totalbits, readbits = 0;
    int i;
    char out[256];
    size_t namelen;
    FILE *fin, *fout;

    fin = fopen(infile, "rb");

    if (!fin) {
        perror("fopen");
        return 1;
    }

    /* check magic number */
    fread(&m, sizeof(m), 1, fin);

    if (m != magic) {
        fprintf(stderr, "Invalid file type.\n");
        return 1;
    }

    fread(&totalbits, sizeof(totalbits), 1, fin);
    fread(&namelen, sizeof(namelen), 1, fin);
    fread(out, sizeof(char), namelen + 1, fin);

    if (outfile) {
        strcpy(outfile, out);
    }

    fout = fopen(out, "wb");

    if (!fout) {
        perror("fopen");
        return 1;
    }

    node = root;

    while (readbits <= totalbits) {
        if (node == nyt) {
            /* reached NYT node, read entire 8-bit symbol */
            symbol = 0;

            for(i = 0; i < bitsof(symbol) - 1; i++) {
                symbol |= read_bit(fin);
                symbol <<= 1;
            }
            symbol |= read_bit(fin);

            readbits += bitsof(symbol);

            fwrite(&symbol, sizeof(symbol), 1, fout);

            node = nyt_spawn(symbol);
            update(node->parent);
            node = root;
        } else if (is_leaf(node)) {
            fwrite(&node->symbol, sizeof(node->symbol), 1, fout);
            update(node);
            node = root;
        } else {
            if (read_bit(fin)) {
                node = node->r_child;
            } else {
                node = node->l_child;
            }

            readbits++;
        }
    }

    fclose(fout);
    fclose(fin);

    return 0;
}

static huffman_node_t *make_node(huffman_node_t *parent)
{
    huffman_node_t *node;

    node = (huffman_node_t *) malloc(sizeof(huffman_node_t));
    node->parent = parent;
    node->l_child = NULL;
    node->r_child = NULL;
    node->weight = 0;

    return node;
}

static huffman_node_t *nyt_spawn(unsigned char symbol)
{
    /* generate two new nodes from NYT node; new NYT is the left node */
    nyt->l_child = make_node(nyt);
    nyt->l_child->id = nyt->id - 2;

    nyt->r_child = make_node(nyt);
    nyt->r_child->id = nyt->id - 1;
    nyt->r_child->symbol = symbol;
    nyt->r_child->weight = 1;
    nyt->weight = 1;

    leaf[symbol] = nyt->r_child;

    nyt = nyt->l_child;

    return nyt->parent;
}

static void update(huffman_node_t *node)
{
    huffman_node_t *highest;

    while (node) {
        /* find highest node with current weight */
        highest = group_highest_id(node->weight, root);

        if (highest &&
            highest != node &&
            highest != node->parent &&
            highest != root)
        {
            swap_nodes(node, highest);
        }

        node->weight++;

        node = node->parent;
    }
}

static
huffman_node_t *group_highest_id(huffman_weight_t weight, huffman_node_t *node)
{
    huffman_node_t *l_highest, *r_highest, *highest_child, *highest;

    if (!node || node->weight < weight) {
        return NULL;
    }

    l_highest = group_highest_id(weight, node->l_child);
    r_highest = group_highest_id(weight, node->r_child);

    if (l_highest && r_highest) {
        highest_child =
            (l_highest->id > r_highest->id ? l_highest : r_highest);
    } else {
        highest_child = (l_highest ? l_highest : r_highest);
    }

    if (node->weight == weight) {
        highest = (highest_child && node->id < highest_child->id ?
                   highest_child : node);
    } else {
        highest = highest_child;
    }

    return highest;
}

static void swap_nodes(huffman_node_t *node1, huffman_node_t *node2)
{
    huffman_node_t tmp;

    tmp = *node1;

    /* update parent nodes first */
    if (is_l_child(node1)) {
        node1->parent->l_child = node2;
    } else {
        node1->parent->r_child = node2;
    }

    if (is_l_child(node2)) {
        node2->parent->l_child = node1;
    } else {
        node2->parent->r_child = node1;
    }

    /* keep IDs in their positions */
    node1->id = node2->id;
    node2->id = tmp.id;

    /* update links to parent nodes */
    node1->parent = node2->parent;
    node2->parent = tmp.parent;
}

static int get_node_path(huffman_node_t *node, unsigned int *path)
{
    unsigned short nbits = 0;

    /* get path to a node in leaf->root direction */
    while (node != root) {
        /* store bits in order of descent (root->leaf) */
        *path >>= 1;

        if (!is_l_child(node)) {
            *path |= 1 << (bitsof(*path) - 1);
        }

        nbits++;
        node = node->parent;
    }

    return nbits;
}

static void write_bits(FILE *fp, unsigned int bits, unsigned short n)
{
    while (n--) {
        write_bit(fp, (bits >> bitsof(bits) - 1) & 1);
        bits <<= 1;
    }
}

void write_bit(FILE *fp, unsigned char bit)
{
    static unsigned char buf = 0, bufbits = 0;

    buf <<= 1;
    buf |= bit;
    bufbits++;

    if (bufbits == bitsof(buf)) {
        fwrite(&buf, sizeof(buf), 1, fp);
        buf = bufbits = 0;
    }
}

static int read_bit(FILE *fp)
{
    static unsigned char buf = 0, bufbits = 0;
    int rc = 0;

    if (!bufbits && !feof(fp)) {
        fread(&buf, sizeof(buf), 1, fp);
        bufbits = bitsof(buf);
    }

    rc = (buf >> (bitsof(buf) - 1)) & 1;

    buf <<= 1;
    bufbits--;

    return rc;
}
