// (c) 2020 minim.co

#ifdef FEATURE_GZIP_REQUESTS // Only if this is feature is enabled
// Wrapper routines from zlib
// Zlib is thread safe
// Can safely be called in parallel
// So no locks are needed

#include "unum.h"

// Compress data using zlib library
// buf - pointer to data to be compressed
// buf_len - length of the data to be compressed
// cbuf - pointer to the buffer where to store compressed data
// cbuf_len - cbuf size
// Returns: length of the compressed data or negative error code
//          if fails
int util_compress(char *buf, int buf_len, char *cbuf, int cbuf_len)
{
    z_stream stream = { .total_in = 0, .total_out = 0 };
    int ret;
    int GZIP_ENCODING = 16;
    int WINDOW_BITS = 15;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    stream.avail_in = buf_len;
    stream.next_in = (Bytef *) buf;
    stream.avail_out = cbuf_len;
    stream.next_out = (Bytef *)cbuf;
    
    // Init the compression system
    if((ret = deflateInit2(&stream, DEFAULT_COMP_ALGO, Z_DEFLATED,
                           WINDOW_BITS | GZIP_ENCODING, 8,
                           Z_DEFAULT_STRATEGY)) != Z_OK)
    {
        log("%s: deflateInit2() failed, error %d\n", __func__, ret);
        return -1;
    }
    // Attempt to compress the data
    ret = deflate(&stream, Z_FINISH);
    if(ret != Z_OK && ret != Z_STREAM_END)
    {
        log("%s: deflate() failed, error %d\n", __func__, ret);
        deflateEnd(&stream);
        return -2;
    }
    ret = deflateEnd(&stream);
    if(ret != Z_OK)
    {
        log("%s: deflateEnd() failed: %d\n", __func__, ret);
        return -3;
    }

    return cbuf_len - stream.avail_out;
}

#endif // FEATURE_GZIP_REQUESTS
