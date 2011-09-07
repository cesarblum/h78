#ifndef HUFFMAN_H
#define HUFFMAN_H

/*
 * Function prototypes.
 */
void huffman_init(void);
int huffman_encode(const char *filename, const char *outfile);
int huffman_decode(const char *filename, char *outfile);

#endif
