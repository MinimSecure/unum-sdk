// (c) 2020 minim.co
// unum zlib utils include file

#ifndef _UTIL_ZLIB_H
#define _UTIL_ZLIB_H

#ifdef FEATURE_GZIP_REQUESTS // Only if this is feature is enabled

#include <zlib.h>

// Default compression algorithm
// Can be overwritten by platform specific code
// For platforms with FUP limits, we can use Z_BEST_SPEED
#define DEFAULT_COMP_ALGO Z_DEFAULT_COMPRESSION

// Compress data using zlib library
// buf - pointer to data to be compressed
// buf_len - length of the data to be compressed
// cbuf - pointer to the buffer where to store compressed data
// cbuf_len - cbuf size
// Returns: length of the compressed data or negative error code
//          if fails
int util_compress(char *msg, int len, char *cmsg, int cmsg_len);

#endif // FEATURE_GZIP_REQUESTS

#endif // _UTIL_ZLIB_H
