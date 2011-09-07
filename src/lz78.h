#ifndef LZ78_H
#define LZ78_H

/*
 * Function prototypes.
 */
int lz78_encode(const char *infile, const char *outfile);
int lz78_decode(const char *infile, char *outfile);

#endif
