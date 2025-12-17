/*
 * z_stream.h
 *     Streaming compression
 */


#ifndef Z_STREAM_H
#define Z_STREAM_H

#include <stdlib.h>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif

/// Ugly hack
#define Assert(condition)

#define ZS_OK (0)
#define ZS_IO_ERROR (-1)
#define ZS_DECOMPRESS_ERROR (-2)
#define ZS_COMPRESS_ERROR (-3)
#define ZS_STREAM_END (-4)
#define ZS_DATA_PENDING (-5)
#define ZS_INCOMPLETE_SRC (-6)	/* cannot decompress unless full src message
								 * is fetched */
#define ZS_EXPAND_DST (-7)		/* cannot compress: dst buffer is too small
								 * for given src size */
#define ZS_BUFFER_ERROR (-8) /* client and server have misconfigured buffer sizes */

struct ZStream;
typedef struct ZStream ZStream;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create compression stream for sending compressed data.
 * c_alg_impl: index of chosen compression algorithm
 * c_level: compression c_level
 */
ZStream * zs_create_compressor(unsigned int c_alg_impl, int c_level);

/*
 * Create decompression stream for reading compressed data.
 * d_alg_impl: index of chosen decompression algorithm
 */
ZStream * zs_create_decompressor(unsigned int d_alg_impl);

/*
 * Read up to "size" raw (decompressed) bytes.
 * Returns number of decompressed bytes or error code.
 * Error code is either ZS_DECOMPRESS_ERROR or error code returned by the rx function.
 */
ssize_t zs_read(ZStream * zs, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed);

/*
 * Write up to "size" raw (decompressed) bytes.
 * Returns number of written raw bytes or error code.
 * Error code is either ZS_COMPRESS_ERROR or error code returned by the tx function.
 * In the last case number of bytes written is stored in *processed.
 */
ssize_t zs_write(ZStream * zs, void const *buf, size_t size, size_t *processed, void *dst, size_t dst_size, size_t *dst_processed);

/*
 * Get decompressor error message.
 */
char const *zs_decompress_error(ZStream * zs);

/*
 * Get compressor error message.
 */
char const *zs_compress_error(ZStream * zs);

/*
 * End the compression stream.
 */
ssize_t zs_end_compression(ZStream * zs, void *dst, size_t dst_size, size_t *dst_processed);

/*
 * Free stream created by zs_create_compressor function.
 */
void zs_compressor_free(ZStream * zs);

/*
 * Free stream created by zs_create_decompressor function.
 */
void zs_decompressor_free(ZStream * zs);

/*
 * Get the name of chosen compression algorithm.
 */
char const *zs_compress_algorithm_name(ZStream * zs);

/*
 * Get the name of chosen decompression algorithm.
 */
char const *zs_decompress_algorithm_name(ZStream * zs);

/*
  Returns zero terminated array with compression algorithms names
*/
char **zs_get_supported_algorithms(void);

/*
  Returns true if provided id is a valid compression algorithm id, otherwise returns false
*/
bool zs_is_valid_impl_id(unsigned int id);

bool zs_buffered(ZStream * zs);

#ifdef __cplusplus
}
#endif

#endif /* Z_STREAM_H */