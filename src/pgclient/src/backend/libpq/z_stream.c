#include "c.h"
#include "pg_config.h"
#include "libpq/z_stream.h"
#include "utils/lz4.h"
#include <zstd.h>

/*
 * Functions implementing streaming compression algorithm
 */
typedef struct
{
	/*
	 * Name of compression algorithm.
	 */
	char const *(*name) (void);

	/*
	 * Create new compression stream. level: compression level
	 */
	void	   *(*create_compressor) (int level);

	/*
	 * Create new decompression stream.
	 */
	void	   *(*create_decompressor) ();

	/*
	 * Decompress up to "src_size" compressed bytes from *src and write up to
	 * "dst_size" raw (decompressed) bytes to *dst. Number of decompressed
	 * bytes written to *dst is stored in *dst_processed. Number of compressed
	 * bytes read from *src is stored in *src_processed.
	 *
	 * Return codes: ZS_OK if no errors were encountered during decompression
	 * attempt. This return code does not guarantee that *src_processed > 0 or
	 * *dst_processed > 0.
	 *
	 * ZS_DATA_PENDING means that there might be some data left within
	 * decompressor internal buffers.
	 *
	 * ZS_STREAM_END if encountered end of compressed data stream.
	 *
	 * ZS_DECOMPRESS_ERROR if encountered an error during decompression
	 * attempt.
	 */
	ssize_t		(*decompress) (void *ds, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed);

	/*
	 * Compress up to "src_size" raw (non-compressed) bytes from *src and
	 * write up to "dst_size" compressed bytes to *dst. Number of compressed
	 * bytes written to *dst is stored in *dst_processed. Number of
	 * non-compressed bytes read from *src is stored in *src_processed.
	 *
	 * Return codes: ZS_OK if no errors were encountered during compression
	 * attempt. This return code does not guarantee that *src_processed > 0 or
	 * *dst_processed > 0.
	 *
	 * ZS_DATA_PENDING means that there might be some data left within
	 * compressor internal buffers.
	 *
	 * ZS_COMPRESS_ERROR if encountered an error during compression attempt.
	 */
	ssize_t		(*compress) (void *cs, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed);

	/*
	 * Free compression stream created by create_compressor function.
	 */
	void		(*free_compressor) (void *cs);

	/*
	 * Free decompression stream created by create_decompressor function.
	 */
	void		(*free_decompressor) (void *ds);

	/*
	 * Get compressor error message.
	 */
	char const *(*compress_error) (void *cs);

	/*
	 * Get decompressor error message.
	 */
	char const *(*decompress_error) (void *ds);

	ssize_t		(*end_compression) (void *cs, void *dst, size_t dst_size, size_t *dst_processed);
}			ZAlgorithm;

struct ZStream
{
	ZAlgorithm const *algorithm;
	void	   *stream;
	bool		not_flushed;
};


#define MESSAGE_MAX_BYTES 8192
#define RING_BUFFER_BYTES (1024 * 64 + MESSAGE_MAX_BYTES)

typedef struct ZS_LZ4_CStream
{
	LZ4_stream_t *stream;
	int			level;
	size_t		buf_pos;
	char		buf[RING_BUFFER_BYTES];
}			ZS_LZ4_CStream;

typedef struct ZS_LZ4_DStream
{
	LZ4_streamDecode_t *stream;
	size_t		buf_pos;
	char		buf[RING_BUFFER_BYTES];
}			ZS_LZ4_DStream;

static void *
lz4_create_compressor(int level)
{
	ZS_LZ4_CStream *c_stream = (ZS_LZ4_CStream *) malloc(sizeof(ZS_LZ4_CStream));

	if (c_stream == NULL)
	{
		return NULL;
	}
	c_stream->stream = LZ4_createStream();
	c_stream->level = level;
	c_stream->buf_pos = 0;
	if (c_stream->stream == NULL)
	{
		free(c_stream);
		return NULL;
	}
	return c_stream;
}

static void *
lz4_create_decompressor()
{
	ZS_LZ4_DStream *d_stream = (ZS_LZ4_DStream *) malloc(sizeof(ZS_LZ4_DStream));

	if (d_stream == NULL)
	{
		return NULL;
	}

	d_stream->stream = LZ4_createStreamDecode();
	d_stream->buf_pos = 0;
	if (d_stream->stream == NULL)
	{
		free(d_stream);
		return NULL;
	}

	return d_stream;
}


static ssize_t
lz4_decompress(void *d_stream, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ZS_LZ4_DStream *ds = (ZS_LZ4_DStream *) d_stream;


	char	   *const decPtr = &ds->buf[ds->buf_pos];

	const int	decBytes = LZ4_decompress_safe_continue(
														ds->stream, (const char*)src, decPtr, (int) src_size, (int) dst_size);

	if (decBytes < 0)
		return ZS_DECOMPRESS_ERROR;


	*dst_processed = decBytes;
	*src_processed = src_size;

	memcpy(dst, decPtr, decBytes);	/* write msg length */

	ds->buf_pos += decBytes;
	if (ds->buf_pos >= RING_BUFFER_BYTES - MESSAGE_MAX_BYTES)
	{
		ds->buf_pos = 0;
	}

	return ZS_OK;
}

static ssize_t
lz4_compress(void *c_stream, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ZS_LZ4_CStream *cs = (ZS_LZ4_CStream *) c_stream;
	int			cmpBytes;



	src_size = Min(MESSAGE_MAX_BYTES, src_size);

	memcpy((char *) (cs->buf) + cs->buf_pos, src, src_size);	/* write msg length */

	Assert(dst_size >= LZ4_compressBound(src_size));

	cmpBytes = LZ4_compress_fast_continue(
										  cs->stream, (const char*) (cs->buf) + cs->buf_pos, (char*)dst, (int) src_size, (int) dst_size, cs->level);


	if (cmpBytes < 0)
		return ZS_DECOMPRESS_ERROR;
	Assert(cmpBytes <= MESSAGE_MAX_BYTES);

	*dst_processed = cmpBytes;
	*src_processed = src_size;

	cs->buf_pos += src_size;
	if (cs->buf_pos >= RING_BUFFER_BYTES - MESSAGE_MAX_BYTES)
	{
		cs->buf_pos = 0;
	}
	return ZS_OK;
}


static ssize_t
lz4_end(void *c_stream, void *dst, size_t dst_size, size_t *dst_processed)
{
	*dst_processed = 0;
	return ZS_OK;
}

static void
lz4_free_compressor(void *c_stream)
{
	ZS_LZ4_CStream *cs = (ZS_LZ4_CStream *) c_stream;

	if (cs != NULL)
	{
		if (cs->stream != NULL)
		{
			LZ4_freeStream(cs->stream);
		}
		free(cs);
	}
}

static void
lz4_free_decompressor(void *d_stream)
{
	ZS_LZ4_DStream *ds = (ZS_LZ4_DStream *) d_stream;

	if (ds != NULL)
	{
		if (ds->stream != NULL)
		{
			LZ4_freeStreamDecode(ds->stream);
		}
		free(ds);
	}
}

static char const *
lz4_error(void *stream)
{
	/* lz4 doesn't have any explicit API to get the error names */
	return "NO_MSG";
}

static char const *
lz4_name(void)
{
	return "lz4";
}






/*
 * Maximum allowed back-reference distance, expressed as power of 2.
 * This setting controls max compressor/decompressor window size.
 * More details https://github.com/facebook/zstd/blob/v1.4.7/lib/zstd.h#L536
 */
#define ZSTD_WINDOWLOG_LIMIT 23 /* set max window size to 8MB */


typedef struct ZS_ZSTD_CStream
{
	ZSTD_CStream *stream;
	char const *error;			/* error message */
}			ZS_ZSTD_CStream;

typedef struct ZS_ZSTD_DStream
{
	ZSTD_DStream *stream;
	char const *error;			/* error message */
}			ZS_ZSTD_DStream;

static void *
zstd_create_compressor(int level)
{
	size_t		rc;
	ZS_ZSTD_CStream *c_stream = (ZS_ZSTD_CStream *) malloc(sizeof(ZS_ZSTD_CStream));

	c_stream->stream = ZSTD_createCStream();
	rc = ZSTD_initCStream(c_stream->stream, level);
	if (ZSTD_isError(rc))
	{
		ZSTD_freeCStream(c_stream->stream);
		free(c_stream);
		return NULL;
	}
#if ZSTD_VERSION_MAJOR > 1 || ZSTD_VERSION_MINOR > 3
	ZSTD_CCtx_setParameter(c_stream->stream, ZSTD_c_windowLog, ZSTD_WINDOWLOG_LIMIT);
#endif
	c_stream->error = NULL;
	return c_stream;
}

static void *
zstd_create_decompressor()
{
	size_t		rc;
	ZS_ZSTD_DStream *d_stream = (ZS_ZSTD_DStream *) malloc(sizeof(ZS_ZSTD_DStream));

	d_stream->stream = ZSTD_createDStream();
	rc = ZSTD_initDStream(d_stream->stream);
	if (ZSTD_isError(rc))
	{
		ZSTD_freeDStream(d_stream->stream);
		free(d_stream);
		return NULL;
	}
#if ZSTD_VERSION_MAJOR > 1 || ZSTD_VERSION_MINOR > 3
	ZSTD_DCtx_setParameter(d_stream->stream, ZSTD_d_windowLogMax, ZSTD_WINDOWLOG_LIMIT);
#endif
	d_stream->error = NULL;
	return d_stream;
}

static ssize_t
zstd_decompress(void *d_stream, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ZS_ZSTD_DStream *ds = (ZS_ZSTD_DStream *) d_stream;
	ZSTD_inBuffer in;
	ZSTD_outBuffer out;
	size_t		rc;

	in.src = src;
	in.pos = 0;
	in.size = src_size;

	out.dst = dst;
	out.pos = 0;
	out.size = dst_size;

	rc = ZSTD_decompressStream(ds->stream, &out, &in);

	*src_processed = in.pos;
	*dst_processed = out.pos;
	if (ZSTD_isError(rc))
	{
		ds->error = ZSTD_getErrorName(rc);
		return ZS_DECOMPRESS_ERROR;
	}

	if (rc == 0)
	{
		return ZS_STREAM_END;
	}

	if (out.pos == out.size)
	{
		/*
		 * if `output.pos == output.size`, there might be some data left
		 * within internal buffers
		 */
		return ZS_DATA_PENDING;
	}
	return ZS_OK;
}

static ssize_t
zstd_compress(void *c_stream, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ZS_ZSTD_CStream *cs = (ZS_ZSTD_CStream *) c_stream;
	ZSTD_inBuffer in;
	ZSTD_outBuffer out;

	in.src = src;
	in.pos = 0;
	in.size = src_size;

	out.dst = dst;
	out.pos = 0;
	out.size = dst_size;

	if (in.pos < src_size)		/* Has something to compress in input buffer */
	{
		size_t		rc = ZSTD_compressStream(cs->stream, &out, &in);

		*dst_processed = out.pos;
		*src_processed = in.pos;
		if (ZSTD_isError(rc))
		{
			cs->error = ZSTD_getErrorName(rc);
			return ZS_COMPRESS_ERROR;
		}
	}

	if (in.pos == src_size)		/* All data is compressed: flush internal zstd
								 * buffer */
	{
		size_t		tx_not_flushed = ZSTD_flushStream(cs->stream, &out);

		*dst_processed = out.pos;
		if (tx_not_flushed > 0)
		{
			return ZS_DATA_PENDING;
		}
	}

	return ZS_OK;
}

static ssize_t
zstd_end(void *c_stream, void *dst, size_t dst_size, size_t *dst_processed)
{
	size_t		tx_not_flushed;
	ZS_ZSTD_CStream *cs = (ZS_ZSTD_CStream *) c_stream;
	ZSTD_outBuffer output;

	output.dst = dst;
	output.pos = 0;
	output.size = dst_size;

	do
	{
		tx_not_flushed = ZSTD_endStream(cs->stream, &output);
	} while ((tx_not_flushed > 0) && (output.pos < output.size));

	*dst_processed = output.pos;

	if (tx_not_flushed > 0)
	{
		return ZS_DATA_PENDING;
	}
	return ZS_OK;
}

static void
zstd_free_compressor(void *c_stream)
{
	ZS_ZSTD_CStream *cs = (ZS_ZSTD_CStream *) c_stream;

	if (cs != NULL)
	{
		ZSTD_freeCStream(cs->stream);
		free(cs);
	}
}

static void
zstd_free_decompressor(void *d_stream)
{
	ZS_ZSTD_DStream *ds = (ZS_ZSTD_DStream *) d_stream;

	if (ds != NULL)
	{
		ZSTD_freeDStream(ds->stream);
		free(ds);
	}
}

static char const *
zstd_compress_error(void *c_stream)
{
	ZS_ZSTD_CStream *cs = (ZS_ZSTD_CStream *) c_stream;

	return cs->error;
}

static char const *
zstd_decompress_error(void *d_stream)
{
	ZS_ZSTD_DStream *ds = (ZS_ZSTD_DStream *) d_stream;

	return ds->error;
}

static char const *
zstd_name(void)
{
	return "zstd";
}






static char const *
no_compression_name(void)
{
	return NULL;
}

/*
 * Array with all supported compression algorithms.
 */
static ZAlgorithm const zs_algorithms[] =
{
	{zstd_name, zstd_create_compressor, zstd_create_decompressor, zstd_decompress, zstd_compress, zstd_free_compressor, zstd_free_decompressor, zstd_compress_error, zstd_decompress_error, zstd_end},
	{lz4_name, lz4_create_compressor, lz4_create_decompressor, lz4_decompress, lz4_compress, lz4_free_compressor, lz4_free_decompressor, lz4_error, lz4_error, lz4_end},
	{no_compression_name}
};

	bool
zs_is_valid_impl_id(unsigned int id)
{
	return id >= 0 && id < lengthof(zs_algorithms);
}

static ssize_t
zs_init_compressor(ZStream * zs, unsigned int c_alg_impl, int c_level)
{
	if (!zs_is_valid_impl_id(c_alg_impl))
	{
		return -1;
	}
	zs->algorithm = &zs_algorithms[c_alg_impl];
	zs->stream = zs->algorithm->create_compressor(c_level);
	if (zs->stream == NULL)
	{
		return -1;
	}
	return 0;
}

static ssize_t
zs_init_decompressor(ZStream * zs, unsigned int d_alg_impl)
{
	if (!zs_is_valid_impl_id(d_alg_impl))
	{
		return -1;
	}
	zs->algorithm = &zs_algorithms[d_alg_impl];
	zs->stream = zs->algorithm->create_decompressor();
	if (zs->stream == NULL)
	{
		return -1;
	}
	return 0;
}

/*
 * Index of used compression algorithm in zs_algorithms array.
 */
ZStream *
zs_create_compressor(unsigned int c_alg_impl, int c_level)
{
	ZStream    *zs = (ZStream *) malloc(sizeof(ZStream));

	zs->not_flushed = false;

	if (zs_init_compressor(zs, c_alg_impl, c_level))
	{
		free(zs);
		return NULL;
	}

	return zs;
}

ZStream *
zs_create_decompressor(unsigned int d_alg_impl)
{
	ZStream    *zs = (ZStream *) malloc(sizeof(ZStream));

	zs->not_flushed = false;

	if (zs_init_decompressor(zs, d_alg_impl))
	{
		free(zs);
		return NULL;
	}

	return zs;
}


ssize_t
zs_read(ZStream * zs, void const *src, size_t src_size, size_t *src_processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ssize_t		rc;

	*src_processed = 0;
	*dst_processed = 0;

	rc = zs->algorithm->decompress(zs->stream,
								   src, src_size, src_processed,
								   dst, dst_size, dst_processed);

	zs->not_flushed = false;
	if (rc == ZS_DATA_PENDING)
	{
		zs->not_flushed = true;
		return ZS_OK;
	}

	if (rc == ZS_OK || rc == ZS_INCOMPLETE_SRC || rc == ZS_STREAM_END)
	{
		return rc;
	}

	return ZS_DECOMPRESS_ERROR;
}

ssize_t
zs_write(ZStream * zs, void const *buf, size_t size, size_t *processed, void *dst, size_t dst_size, size_t *dst_processed)
{
	ssize_t		rc;

	*processed = 0;
	*dst_processed = 0;

	rc = zs->algorithm->compress(zs->stream,
								 buf, size, processed,
								 dst, dst_size, dst_processed);

	zs->not_flushed = false;
	if (rc == ZS_DATA_PENDING)
	{
		zs->not_flushed = true;
		return ZS_OK;
	}
	if (rc != ZS_OK)
	{
		return ZS_COMPRESS_ERROR;
	}

	return rc;
}

void
zs_compressor_free(ZStream * zs)
{
	if (zs == NULL)
	{
		return;
	}

	if (zs->stream)
	{
		zs->algorithm->free_compressor(zs->stream);
	}

	free(zs);
}

void
zs_decompressor_free(ZStream * zs)
{
	if (zs == NULL)
	{
		return;
	}

	if (zs->stream)
	{
		zs->algorithm->free_decompressor(zs->stream);
	}

	free(zs);
}

ssize_t
zs_end_compression(ZStream * zs, void *dst, size_t dst_size, size_t *dst_processed)
{
	ssize_t		rc;

	*dst_processed = 0;

	rc = zs->algorithm->end_compression(zs->stream, dst, dst_size, dst_processed);

	zs->not_flushed = false;
	if (rc == ZS_DATA_PENDING)
	{
		zs->not_flushed = true;
		return ZS_OK;
	}
	if (rc != ZS_OK)
	{
		return ZS_COMPRESS_ERROR;
	}

	return rc;
}

char const *
zs_compress_error(ZStream * zs)
{
	return zs->algorithm->compress_error(zs->stream);
}

char const *
zs_decompress_error(ZStream * zs)
{
	return zs->algorithm->decompress_error(zs->stream);
}

bool
zs_buffered(ZStream * zs)
{
	return zs ? zs->not_flushed : 0;
}


/*
 * Get list of the supported algorithms.
 */
char	  **
zs_get_supported_algorithms(void)
{
	size_t		n_algorithms = lengthof(zs_algorithms);
	char	  **algorithm_names = (char **)malloc(n_algorithms * sizeof(char *));

	for (size_t i = 0; i < n_algorithms; i++)
	{
		algorithm_names[i] = (char *) zs_algorithms[i].name();
	}

	return algorithm_names;
}

char const *
zs_compress_algorithm_name(ZStream * zs)
{
	return zs ? zs->algorithm->name() : NULL;
}

char const *
zs_decompress_algorithm_name(ZStream * zs)
{
	return zs ? zs->algorithm->name() : NULL;
}
