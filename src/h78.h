#ifndef H78_H
#define H78_H

/*
 * Macro definitions.
 */
#ifndef bitsof
#define bitsof(type) (sizeof(type) * 8)
#endif

#define COMPRESS    0x01
#define LZ78        0x02
#define DECOMPRESS  0x04

#endif
