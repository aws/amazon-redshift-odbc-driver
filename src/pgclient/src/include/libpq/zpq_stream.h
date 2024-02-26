/*
 * zpq_stream.h
 *     Streaming compression for libpq
 */
#include "z_stream.h"

#ifndef ZPQ_STREAM_H
#define ZPQ_STREAM_H

#define ZPQ_DEFAULT_COMPRESSION_LEVEL (1)
#define ZPQ_INCOMPLETE_HEADER (-6)
#define ZPQ_FATAL_ERROR (-7)
struct ZpqStream;
typedef struct ZpqStream ZpqStream;

typedef ssize_t (*zpq_tx_func) (void *arg, void const *data, size_t size);
typedef ssize_t (*zpq_rx_func) (void *arg, void *data, size_t size);

/*
 * Descriptor of compression algorithm chosen by client
 */
typedef struct zpq_compressor
{
	unsigned int impl;			/* compression algorithm index */
	int			level;			/* compression level */
}			zpq_compressor;

#endif

/*
 * Create compression stream with rx/tx function for reading/sending compressed data.
 * tx_func: function for writing compressed data in underlying stream
 * rx_func: function for reading compressed data from underlying stream
 * arg: context passed to the function
 * rx_data: received data (compressed data already fetched from input stream)
 * rx_data_size: size of data fetched from input stream
 */
ZpqStream * zpq_create(zpq_compressor * compressors, size_t n_compressors, zpq_tx_func tx_func, zpq_rx_func rx_func, void *arg, char *rx_data, size_t rx_data_size);

/*
 * Write up to "src_size" raw (decompressed) bytes.
 * Returns number of written raw bytes or error code.
 * Error code is either ZPQ_COMPRESS_ERROR or error code returned by the tx function.
 * In the last case number of bytes written is stored in *src_processed.
 */
ssize_t zpq_write(ZpqStream * zpq, void const *src, size_t src_size, size_t *src_processed);

/*
 * Read up to "dst_size" raw (decompressed) bytes.
 * Returns number of decompressed bytes or error code.
 * Error code is either ZPQ_DECOMPRESS_ERROR or error code returned by the rx function.
 */
ssize_t zpq_read(ZpqStream * zpq, void *dst, size_t dst_size, bool noblock);

/*
 * Return true if non-flushed data left in internal rx decompression buffer.
 */
bool zpq_buffered_rx(ZpqStream * zpq);

/*
 * Return true if non-flushed data left in internal tx compression buffer.
 */
bool zpq_buffered_tx(ZpqStream * zpq);

/*
 * Free stream created by zs_create function.
 */
void zpq_free(ZpqStream * zpq);

/*
 * Get decompressor error message.
 */
char const *zpq_decompress_error(ZpqStream * zpq);

/*
 * Get compressor error message.
 */
char const *zpq_compress_error(ZpqStream * zpq);

/*
 * Get the name of the current compression algorithm.
 */
char const *zpq_compress_algorithm_name(ZpqStream * zpq);

/*
 * Get the name of the current decompression algorithm.
 */
char const *zpq_decompress_algorithm_name(ZpqStream * zpq);

/*
 * Parse the compression setting. Returns:
 * - 1 if the compression setting is valid
 * - 0 if the compression setting is valid but disabled
 * - -1 if the compression setting is invalid
 * It also populates the compressors array with the recognized compressors. Size of the array is stored in n_compressors.
 * If no supported compressors recognized or if compression is disabled, then NULL is assigned to *compressors and n_compressors is set to 0.
 */
int
			zpq_parse_compression_setting(char *val, zpq_compressor * *compressors, size_t *n_compressors);

/* Serialize the compressors array to string so it can be transmitted to the other side during the compression startup.
 * For example, for array of two compressors (zstd, level 1), (zlib, level 2) resulting string would look like "zstd:1,zlib:2".
 * Returns the resulting string.
 */
char
		   *zpq_serialize_compressors(zpq_compressor const *compressors, size_t n_compressors);

/* Deserialize the compressors string received during the compression setup to a compressors array.
 * For example, for string "zstd:1,zlib:2" compressors would be populated with 2 elements: (zstd, level 1), (zlib, level 2).
 * Returns:
 * - true if the compressors string is successfully parsed
 * - false otherwise
 * It also populates the compressors array with the recognized compressors. Size of the array is stored in n_compressors.
 * If no supported compressors recognized or string is empty, then NULL is assigned to *compressors and n_compressors is set to 0.
 */
bool
			zpq_deserialize_compressors(char const *c_string, zpq_compressor * *compressors, size_t *n_compressors);

/* Return the currently enabled compression algorithms */
char	   *zpq_algorithms(ZpqStream * zpq);
