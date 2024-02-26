#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif
#include <unistd.h>
#include <math.h>

#include "libpq/zpq_stream.h"
#include "pg_config.h"
#include "port/pg_bswap.h"

/* log warnings on backend */
#ifndef FRONTEND
#define pg_log_warning(...) elog(WARNING, __VA_ARGS__)
#else
#define pg_log_warning(...) (void)0
#endif

/* ZpqBuffer size, in bytes */
#define ZPQ_BUFFER_SIZE 8192000 // This change is only neccesary to match PP version server. In future use 81920.
/* CompressedData msg_type */
#define ZPQ_COMPRESSED_MSG_TYPE 'm'
/* SetCompressionMethod msg_type */
#define ZPQ_SET_COMPRESSION_MSG_TYPE 'k'
/* CompressionAck msg_type */
#define ZPQ_COMPRESSION_ACK_MSG_TYPE 'z'

#define ZPQ_COMPRESS_THRESHOLD 60

#define MAX_DATA_TRUNK 262144 // 2^18, each compressed message should not exceed this length

typedef struct ZpqBuffer ZpqBuffer;


/* ZpqBuffer used as RX/TX buffer in ZpqStream */
struct ZpqBuffer
{
	char		buf[ZPQ_BUFFER_SIZE];
	size_t		size;			/* current size of buf */
	size_t		pos;			/* current position in buf, in range [0, size] */
};

static inline void
zpq_buf_init(ZpqBuffer * zb)
{
	zb->size = 0;
	zb->pos = 0;
}

static inline size_t
zpq_buf_left(ZpqBuffer * zb)
{
	Assert(zb->buf);
	return ZPQ_BUFFER_SIZE - zb->size;
}

static inline size_t
zpq_buf_unread(ZpqBuffer * zb)
{
	return zb->size - zb->pos;
}

static inline char *
zpq_buf_size(ZpqBuffer * zb)
{
	return (char *) (zb->buf) + zb->size;
}

static inline char *
zpq_buf_pos(ZpqBuffer * zb)
{
	return (char *) (zb->buf) + zb->pos;
}

static inline void
zpq_buf_size_advance(ZpqBuffer * zb, size_t value)
{
	zb->size += value;
}

static inline void
zpq_buf_pos_advance(ZpqBuffer * zb, size_t value)
{
	zb->pos += value;
}

static inline void
zpq_buf_reuse(ZpqBuffer * zb)
{
	size_t		unread = zpq_buf_unread(zb);

	if (unread > 5)				/* can read message header, don't do anything */
		return;
	if (unread == 0)
	{
		zb->size = 0;
		zb->pos = 0;
		return;
	}
	memmove(zb->buf, zb->buf + zb->pos, unread);
	zb->size = unread;
	zb->pos = 0;
}

struct ZpqStream
{
	ZStream    *c_stream;		/* underlying compression stream */
	ZStream    *d_stream;		/* underlying decompression stream */

	size_t		tx_total;		/* amount of bytes sent to tx_func */

	size_t		tx_total_raw;	/* amount of bytes received by zpq_write */
	size_t		rx_total;		/* amount of bytes read by rx_func */
	size_t		rx_total_raw;	/* amount of bytes returned by zpq_write */
	bool		is_compressing; /* current compression state */

	bool		is_decompressing;	/* current decompression state */
	size_t		rx_msg_bytes_left;	/* number of bytes left to process without
									 * changing the decompression state */
	size_t		tx_msg_bytes_left;	/* number of bytes left to process without
									 * changing the compression state */

	ZpqBuffer	rx_in;			/* buffer for unprocessed data read by rx_func */
	ZpqBuffer	tx_in;			/* buffer for unprocessed data consumed by
								 * zpq_write */
	ZpqBuffer	tx_out;			/* buffer for processed data waiting for send
								 * via tx_func */

	zpq_rx_func rx_func;
	zpq_tx_func tx_func;
	void	   *arg;

	zpq_compressor *compressors;	/* compressors array holds the available
									 * compressors to use for
									 * compression/decompression */
	size_t		n_compressors;	/* size of the compressors array */
	int			compress_alg_idx;	/* index of the active compression
									 * algorithm */
	int			decompress_alg_idx; /* index of the active decompression
									 * algorithm */
	int			compressor_by_msg_type[256];	/* map to choose a compressor
												 * by the protocol message
												 * type */

	bool		reading_set_compression;	/* utility marker indicating
											 * partial SetCompressionMethod
											 * read */
};

/*
 * Interpret Compression Ack after NFT failover
 */
static void
zpq_restart(ZpqStream * zpq)
{
	zs_decompressor_free(zpq->d_stream);
	zpq->decompress_alg_idx = -1;
	zs_compressor_free(zpq->c_stream);
	zpq->compress_alg_idx = -1;
}

/*
 * Message compression map defines the logic for choosing the compressor
 * based on the protocol message type. Currently, it is a basic prototype to demonstrate
 * the capabilities of the on-the-fly compression switch.
 */
static inline void
zpq_build_msg_compression_map(ZpqStream * zpq)
{
	int			i;

	for (i = 0; i < 256; i++)
	{
		zpq->compressor_by_msg_type[i] = -1;
	}

	for (i = 0; i < zpq->n_compressors; i++)
	{
		/* compress CopyData, DataRow and Query messages */
		if (zpq->compressor_by_msg_type['d'] == -1)
		{
			zpq->compressor_by_msg_type['d'] = i;
		}
		if (zpq->compressor_by_msg_type['D'] == -1)
		{
			zpq->compressor_by_msg_type['D'] = i;
		}
		if (zpq->compressor_by_msg_type['Q'] == -1)
		{
			zpq->compressor_by_msg_type['Q'] = i;
		}
	}
}

/*
 * Choose the index of compressor to use for the message of msg_type with msg_len.
 * Return values:
 * - the non-negative index of zpq->compressors array
 * - -1, if message should not be compressed
 */
static inline int
zpq_choose_compressor(ZpqStream * zpq, char msg_type, uint32 msg_len)
{
	if (msg_len >= ZPQ_COMPRESS_THRESHOLD)
	{
		return zpq->compressor_by_msg_type[(unsigned char) msg_type];
	}
	return -1;
}

/*
 * Check if should compress message of msg_type with msg_len.
 * Return true if should, false if should not.
 */
static inline bool
zpq_should_compress(ZpqStream * zpq, char msg_type, uint32 msg_len)
{
	// return zpq_choose_compressor(zpq, msg_type, msg_len) != -1;
	// Disable compression for Tx until further testing can be completed.
	return false;

}

/*
 * Check if message is a CompressedData.
 * Return true if it is, otherwise false.
 * */
static inline bool
zpq_is_compressed_msg(char msg_type)
{
	return msg_type == ZPQ_COMPRESSED_MSG_TYPE;
}

/*
 * Check if message is a CompressionAck.
 */
static inline bool
zpq_is_compression_ack(char msg_type)
{
	return msg_type == ZPQ_COMPRESSION_ACK_MSG_TYPE;
}

/*
 * Check if message is a SetCompressionMethod.
 * Return true if it is, otherwise false.
 * */
static inline bool
zpq_is_set_compression_msg(char msg_type)
{
	return msg_type == ZPQ_SET_COMPRESSION_MSG_TYPE;
}

ZpqStream *
zpq_create(zpq_compressor * compressors, size_t n_compressors, zpq_tx_func tx_func, zpq_rx_func rx_func, void *arg, char *rx_data, size_t rx_data_size)
{
	ZpqStream  *zpq;

	/* zpqStream needs at least one compressor */
	if (n_compressors == 0 || compressors == NULL)
	{
		return NULL;
	}
	zpq = (ZpqStream *) malloc(sizeof(ZpqStream));

	zpq->compressors = compressors;
	zpq->n_compressors = n_compressors;
	zpq->compress_alg_idx = -1;
	zpq->decompress_alg_idx = -1;

	zpq->is_compressing = false;
	zpq->is_decompressing = false;
	zpq->rx_msg_bytes_left = 0;
	zpq->tx_msg_bytes_left = 0;
	zpq_buf_init(&zpq->tx_in);

	zpq->tx_total = 0;
	zpq->tx_total_raw = 0;
	zpq->rx_total = 0;
	zpq->rx_total_raw = 0;

	zpq_buf_init(&zpq->rx_in);
	zpq_buf_size_advance(&zpq->rx_in, rx_data_size);
	Assert(rx_data_size < ZPQ_BUFFER_SIZE);
	memcpy(zpq->rx_in.buf, rx_data, rx_data_size);

	zpq_buf_init(&zpq->tx_out);

	zpq->rx_func = rx_func;
	zpq->tx_func = tx_func;
	zpq->arg = arg;
	zpq->reading_set_compression = false;
	zpq->c_stream = NULL;
	zpq->d_stream = NULL;

	zpq_build_msg_compression_map(zpq);

	return zpq;
}

/* Compress up to src_size bytes from *src into CompressedData and write it to the tx buffer.
 * Returns ZS_OK on success, ZS_COMPRESS_ERROR if encountered a compression error. */
static inline ssize_t
zpq_write_compressed_message(ZpqStream * zpq, char const *src, size_t src_size, size_t *src_processed)
{
	size_t		compressed_len;
	ssize_t		rc;
	uint32		size;

	/* check if have enough space */
	if (zpq_buf_left(&zpq->tx_out) <= 5)
	{
		/* too little space for CompressedData, abort */
		*src_processed = 0;
		return ZS_OK;
	}

	compressed_len = 0;
	rc = zs_write(zpq->c_stream, src, src_size, src_processed,
				  zpq_buf_size(&zpq->tx_out) + 5, zpq_buf_left(&zpq->tx_out) - 5, &compressed_len);

	if (compressed_len > 0)
	{
		/* write CompressedData type */
		*zpq_buf_size(&zpq->tx_out) = ZPQ_COMPRESSED_MSG_TYPE;
		size = pg_hton32(compressed_len + 4);

		memcpy(zpq_buf_size(&zpq->tx_out) + 1, &size, sizeof(uint32));	/* write msg length */
		compressed_len += 5;	/* append header length to compressed data
								 * length */
	}

	zpq->tx_total_raw += *src_processed;
	zpq->tx_total += compressed_len;
	zpq_buf_size_advance(&zpq->tx_out, compressed_len);
	return rc;
}

/* Copy the data directly from *src to the tx buffer */
static void
zpq_write_uncompressed(ZpqStream * zpq, char const *src, size_t src_size, size_t *src_processed)
{
	src_size = Min(zpq_buf_left(&zpq->tx_out), src_size);
	memcpy(zpq_buf_size(&zpq->tx_out), src, src_size);

	zpq->tx_total_raw += src_size;
	zpq->tx_total += src_size;
	zpq_buf_size_advance(&zpq->tx_out, src_size);
	*src_processed = src_size;
}

static ssize_t
zpq_write_set_compression_msg(ZpqStream * zpq, int new_compress_idx)
{
	uint32		len;
	uint8		idx;

	/*
	 * check if have enough space: msg_type(1 byte) + msg_len(4 bytes) +
	 * compress_alg_idx(1 byte)
	 */
	if (zpq_buf_left(&zpq->tx_out) < 6)
	{
		return -1;
	}

	/* write CompressedData type */
	*zpq_buf_size(&zpq->tx_out) = ZPQ_SET_COMPRESSION_MSG_TYPE;
	len = pg_hton32(5);
	memcpy(zpq_buf_size(&zpq->tx_out) + 1, &len, sizeof(uint32));	/* write msg length */

	/* currently we expect idx to be in range [0, 255] */
	Assert(new_compress_idx >= 0 && new_compress_idx <= UINT8_MAX);
	idx = (uint8) new_compress_idx;
	memcpy(zpq_buf_size(&zpq->tx_out) + 5, &idx, sizeof(uint8));	/* write
																	 * new_compress_idx */

	zpq->tx_total_raw += 6;
	zpq->tx_total += 6;
	zpq_buf_size_advance(&zpq->tx_out, 6);
	return 0;
}

/* Determine if should compress the next message and change the current compression state */
static ssize_t
zpq_toggle_compression(ZpqStream * zpq, char msg_type, uint32 msg_len)
{
	int			new_compress_idx = zpq_choose_compressor(zpq, msg_type, msg_len);
	bool		should_compress = new_compress_idx != -1;

	/*
	 * negative new_compress_idx indicates that we should not compress this
	 * message
	 */
	if (should_compress)
	{
		/*
		 * if the new compressor does not match the current one, process the
		 * switch
		 */
		if (zpq->compress_alg_idx != new_compress_idx)
		{
			if (zpq_write_set_compression_msg(zpq, new_compress_idx))
			{
				/*
				 * come back later when we can write the entire
				 * SetCompressionMethod message
				 */
				return 0;
			}

			zs_compressor_free(zpq->c_stream);
			zpq->c_stream = zs_create_compressor(zpq->compressors[new_compress_idx].impl, zpq->compressors[new_compress_idx].level);
			if (zpq->c_stream == NULL)
			{
				return ZPQ_FATAL_ERROR;
			}
			zpq->compress_alg_idx = new_compress_idx;
		}
	}

	zpq->is_compressing = should_compress;
	zpq->tx_msg_bytes_left = msg_len + 1;
	return 0;
}

/*
 * Internal write function. Reads the data from *src buffer,
 * determines the postgres messages type and length.
 * If message matches the compression criteria, it wraps the message into
 * CompressedData. Otherwise, leaves the message unchanged.
 * If *src data ends with incomplete message header, this function is not
 * going to read this message header.
 * Returns number of written raw bytes or error code.
 * In the last case number of bytes written is stored in *processed.
 */
static ssize_t
zpq_write_internal(ZpqStream * zpq, void const *src, size_t src_size, size_t *processed)
{
	size_t		src_pos = 0;
	ssize_t		rc;

	do
	{
		/*
		 * try to read ahead the next message types and increase
		 * tx_msg_bytes_left, if possible
		 */
		while (zpq->tx_msg_bytes_left > 0 && src_size - src_pos >= zpq->tx_msg_bytes_left + 5)
		{
			char		msg_type = *((char *) src + src_pos + zpq->tx_msg_bytes_left);
			uint32		msg_len;

			memcpy(&msg_len, (char *) src + src_pos + zpq->tx_msg_bytes_left + 1, 4);
			msg_len = pg_ntoh32(msg_len);
			if (zpq_should_compress(zpq, msg_type, msg_len) != zpq->is_compressing)
			{
				/*
				 * cannot proceed further, encountered compression toggle
				 * point
				 */
				break;
			}
			zpq->tx_msg_bytes_left += msg_len + 1;
		}

		/*
		 * Write CompressedData if currently is compressing or have some
		 * buffered data left in underlying compression stream
		 */
		if (zs_buffered(zpq->c_stream) || (zpq->is_compressing && zpq->tx_msg_bytes_left > 0))
		{
			size_t		buf_processed = 0;
			size_t		to_compress = Min(zpq->tx_msg_bytes_left, src_size - src_pos);

			rc = zpq_write_compressed_message(zpq, (char *) src + src_pos, to_compress, &buf_processed);
			src_pos += buf_processed;
			zpq->tx_msg_bytes_left -= buf_processed;

			if (rc != ZS_OK)
			{
				*processed = src_pos;
				return rc;
			}
		}

		/*
		 * If not going to compress the data from *src, just write it
		 * uncompressed.
		 */
		else if (zpq->tx_msg_bytes_left > 0)
		{						/* determine next message type */
			size_t		copy_len = Min(src_size - src_pos, zpq->tx_msg_bytes_left);
			size_t		copy_processed = 0;

			zpq_write_uncompressed(zpq, (char *) src + src_pos, copy_len, &copy_processed);
			src_pos += copy_processed;
			zpq->tx_msg_bytes_left -= copy_processed;
		}

		/*
		 * Reached the compression toggle point, fetch next message header to
		 * determine compression state.
		 */
		else
		{
			char		msg_type;
			uint32		msg_len;

			if (src_size - src_pos < 5)
			{
				/*
				 * must return here because we can't continue without full
				 * message header
				 */
				*processed = src_pos;
				return ZPQ_INCOMPLETE_HEADER;
			}

			msg_type = *((char *) src + src_pos);
			memcpy(&msg_len, (char *) src + src_pos + 1, 4);
			msg_len = pg_ntoh32(msg_len);
			rc = zpq_toggle_compression(zpq, msg_type, msg_len);
			if (rc)
			{
				return rc;
			}
		}

		/*
		 * repeat sending while there is some data in input or internal
		 * compression buffer
		 */
	} while (src_pos < src_size && zpq_buf_left(&zpq->tx_out) > 6);

	return src_pos;
}

ssize_t
zpq_write(ZpqStream * zpq, void const *src, size_t src_size, size_t *src_processed)
{
	size_t		src_pos = 0;
	ssize_t		rc;

	/* try to process as much data as possible before calling the tx_func */
	while (zpq_buf_left(&zpq->tx_out) > 6)
	{
		size_t		copy_len = Min(zpq_buf_left(&zpq->tx_in), src_size - src_pos);
		size_t		processed;

		memcpy(zpq_buf_size(&zpq->tx_in), (char *) src + src_pos, copy_len);
		zpq_buf_size_advance(&zpq->tx_in, copy_len);
		src_pos += copy_len;

		if (zpq_buf_unread(&zpq->tx_in) == 0 && !zs_buffered(zpq->c_stream))
		{
			break;
		}

		processed = 0;

		rc = zpq_write_internal(zpq, zpq_buf_pos(&zpq->tx_in), zpq_buf_unread(&zpq->tx_in), &processed);
		if (rc > 0)
		{
			zpq_buf_pos_advance(&zpq->tx_in, rc);
			zpq_buf_reuse(&zpq->tx_in);
		}
		else
		{
			zpq_buf_pos_advance(&zpq->tx_in, processed);
			zpq_buf_reuse(&zpq->tx_in);
			if (rc == ZPQ_INCOMPLETE_HEADER)
			{
				break;
			}
			*src_processed = src_pos;
			return rc;
		}
	}

	/*
	 * call the tx_func if have any bytes to send
	 */
	while (zpq_buf_unread(&zpq->tx_out))
	{
		rc = zpq->tx_func(zpq->arg, zpq_buf_pos(&zpq->tx_out), zpq_buf_unread(&zpq->tx_out));
		if (rc > 0)
		{
			zpq_buf_pos_advance(&zpq->tx_out, rc);
		}
		else
		{
			*src_processed = src_pos;
			zpq_buf_reuse(&zpq->tx_out);
			return rc;
		}
	}

	zpq_buf_reuse(&zpq->tx_out);
	return src_pos;
}

/* Decompress bytes from RX buffer and write up to dst_len of uncompressed data to *dst.
 * Returns:
 * ZS_OK on success,
 * ZS_STREAM_END if reached end of compressed chunk
 * ZS_DECOMPRESS_ERROR if encountered a decompression error */
static inline ssize_t
zpq_read_compressed_message(ZpqStream * zpq, char *dst, size_t dst_len, size_t *dst_processed)
{
	size_t		rx_processed = 0;
	ssize_t		rc;
	size_t		read_len = Min(zpq->rx_msg_bytes_left, zpq_buf_unread(&zpq->rx_in));

	Assert(read_len == zpq->rx_msg_bytes_left);
	rc = zs_read(zpq->d_stream, zpq_buf_pos(&zpq->rx_in), read_len, &rx_processed,
				 dst, dst_len, dst_processed);

	zpq_buf_pos_advance(&zpq->rx_in, rx_processed);
	zpq->rx_total_raw += *dst_processed;
	zpq->rx_msg_bytes_left -= rx_processed;
	return rc;
}

/* Copy up to dst_len bytes from rx buffer to *dst.
 * Returns amount of bytes copied. */
static inline size_t
zpq_read_uncompressed(ZpqStream * zpq, char *dst, size_t dst_len)
{
	size_t		copy_len;

	Assert(zpq_buf_unread(&zpq->rx_in) > 0);
	copy_len = Min(zpq->rx_msg_bytes_left, Min(zpq_buf_unread(&zpq->rx_in), dst_len));

	memcpy(dst, zpq_buf_pos(&zpq->rx_in), copy_len);

	zpq_buf_pos_advance(&zpq->rx_in, copy_len);
	zpq->rx_total_raw += copy_len;
	zpq->rx_msg_bytes_left -= copy_len;
	return copy_len;
}

/* Determine if should decompress the next message and
 * change the current decompression state */
static inline void
zpq_toggle_decompression(ZpqStream * zpq)
{
	uint32		msg_len;
	char		msg_type = *zpq_buf_pos(&zpq->rx_in);

	memcpy(&msg_len, zpq_buf_pos(&zpq->rx_in) + 1, 4);
	msg_len = pg_ntoh32(msg_len);

	if (zpq_is_set_compression_msg(msg_type))
	{
		Assert(msg_len == 5);
		zpq->reading_set_compression = true;
		/* set compression message header is no longer needed, just skip it */
		zpq_buf_pos_advance(&zpq->rx_in, 5);
	}
	else if (zpq_is_compression_ack(msg_type))
	{
		/*
		 * Got a failover signal from NFT.
		 */
		zpq_restart(zpq);
		zpq_buf_pos_advance(&zpq->rx_in, msg_len + 1);
	}
	else
	{
		zpq->is_decompressing = zpq_is_compressed_msg(msg_type);
		zpq->rx_msg_bytes_left = msg_len + 1;

		if (zpq->is_decompressing)
		{
			/* compressed message header is no longer needed, just skip it */
			zpq_buf_pos_advance(&zpq->rx_in, 5);
			zpq->rx_msg_bytes_left -= 5;
		}
	}
}

static inline ssize_t
zpq_process_switch(ZpqStream * zpq)
{
	uint8		algorithm_idx;

	if (zpq_buf_unread(&zpq->rx_in) < 1)
	{
		return 0;
	}

	algorithm_idx = *zpq_buf_pos(&zpq->rx_in);

	zpq_buf_pos_advance(&zpq->rx_in, 1);
	zpq->reading_set_compression = false;

	if (algorithm_idx != zpq->decompress_alg_idx)
	{
		zs_decompressor_free(zpq->d_stream);
		zpq->d_stream = zs_create_decompressor(zpq->compressors[algorithm_idx].impl);
		if (zpq->d_stream == NULL)
		{
			return ZPQ_FATAL_ERROR;
		}
		zpq->decompress_alg_idx = algorithm_idx;
	}

	return 0;
}

ssize_t
zpq_read(ZpqStream * zpq, void *dst, size_t dst_size, bool noblock)
{
	size_t		dst_pos = 0;
	size_t		dst_processed = 0;
	ssize_t		rc = 0;
	bool 		skip_read = false; 		
	/* Read until some data fetched */
	while (dst_pos == 0)
	{
		zpq_buf_reuse(&zpq->rx_in);
		if (!zpq_buffered_rx(zpq) || (zpq->is_decompressing && zpq_buf_unread(&zpq->rx_in) < zpq->rx_msg_bytes_left))
		{
			if (noblock)
			{
				/*
				 * can't read anything w/o the potentially blocking backend
				 * call
				 */
				return dst_pos;
			}
			if (zpq_buf_left(&zpq->rx_in) == 0)
				return ZS_BUFFER_ERROR;

			skip_read = false;
			rc = 0 ;
			/* One message is not longer than MAX_DATA_TRUNK */
			if( ( zpq_buf_left(&zpq->rx_in) <= MAX_DATA_TRUNK ) )
			{
				/* When buffer pointer is near the end, */
				if( zpq_buf_unread(&zpq->rx_in) < (zpq->rx_msg_bytes_left+1)  ){
					/* When there is NOT a complete message, read only that length to complete one message. */
					/* Side effect: buffer pointer will be reset by zpq_buf_reuse(...) */
					/* If buffer pointer is not reset, function will return ZS_BUFFER_ERROR when pointer reach the end.*/
					/* Previously, network packet and message don't align, so pointer doesn't get reset. */
					rc = zpq->rx_func(zpq->arg, zpq_buf_size(&zpq->rx_in), Min( zpq->rx_msg_bytes_left - zpq_buf_unread(&zpq->rx_in) +1 ,  zpq_buf_left(&zpq->rx_in) )  );
				}
				else{
					/* When there is at least 1 complete message, read nothing. */
					skip_read = true;
				}
			}	
			else{
					/* normal case: buffer pointer is far from the end, read as many as it can */
					rc = zpq->rx_func(zpq->arg, zpq_buf_size(&zpq->rx_in), zpq_buf_left(&zpq->rx_in) - MAX_DATA_TRUNK  );
			}

			if (rc > 0 )			/* read fetches some data */
			{
				zpq->rx_total += rc;
				zpq_buf_size_advance(&zpq->rx_in, rc);
			}
			else if( !skip_read )				/* read failed */
			{
				return rc;
			}
		}

		/*
		 * try to read ahead the next message types and increase
		 * rx_msg_bytes_left, if possible (ONLY UNCOMPRESSED MESSAGES)
		 */
		while (!zpq->is_decompressing && zpq->rx_msg_bytes_left > 0 && (zpq_buf_unread(&zpq->rx_in) >= zpq->rx_msg_bytes_left + 5))
		{
			char		msg_type;
			uint32		msg_len;

			msg_type = *(zpq_buf_pos(&zpq->rx_in) + zpq->rx_msg_bytes_left);
			if (zpq_is_compressed_msg(msg_type) || zpq_is_set_compression_msg(msg_type)
				|| zpq_is_compression_ack(msg_type))
			{
				/*
				 * cannot proceed further, encountered compression toggle
				 * point
				 */
				break;
			}

			memcpy(&msg_len, zpq_buf_pos(&zpq->rx_in) + zpq->rx_msg_bytes_left + 1, 4);
			zpq->rx_msg_bytes_left += pg_ntoh32(msg_len) + 1;
		}


		if (zpq->rx_msg_bytes_left > 0 || zs_buffered(zpq->d_stream))
		{
			dst_processed = 0;
			if (zpq->is_decompressing || zs_buffered(zpq->d_stream))
			{
				if (!zs_buffered(zpq->d_stream) && zpq_buf_unread(&zpq->rx_in) < zpq->rx_msg_bytes_left)
				{
					/*
					 * prefer to read only the fully compressed messages or
					 * read if some data is buffered
					 */
					continue;
				}
				rc = zpq_read_compressed_message(zpq, (char*)dst, dst_size - dst_pos, &dst_processed);
				dst_pos += dst_processed;
				if (rc == ZS_STREAM_END)
				{
					continue;
				}
				if (rc != ZS_OK)
				{
					return rc;
				}
			}
			else
				dst_pos += zpq_read_uncompressed(zpq, (char*)dst, dst_size - dst_pos);
		}
		else if (zpq->reading_set_compression)
		{
			zpq_process_switch(zpq);
		}
		else if (zpq_buf_unread(&zpq->rx_in) >= 5)
			zpq_toggle_decompression(zpq);
	}
	return dst_pos;
}

bool
zpq_buffered_rx(ZpqStream * zpq)
{
	return zpq ? zpq_buf_unread(&zpq->rx_in) >= 5 || (zpq_buf_unread(&zpq->rx_in) > 0 && zpq->rx_msg_bytes_left > 0) ||
		zs_buffered(zpq->d_stream) : 0;
}

bool
zpq_buffered_tx(ZpqStream * zpq)
{
	return zpq ? zpq_buf_unread(&zpq->tx_in) >= 5 || (zpq_buf_unread(&zpq->tx_in) > 0 && zpq->tx_msg_bytes_left > 0) || zpq_buf_unread(&zpq->tx_out) > 0 ||
		zs_buffered(zpq->c_stream) : 0;
}

void
zpq_free(ZpqStream * zpq)
{
	if (zpq)
	{
		if (zpq->c_stream)
		{
			zs_compressor_free(zpq->c_stream);
		}
		if (zpq->d_stream)
		{
			zs_decompressor_free(zpq->d_stream);
		}
		free(zpq);
	}
}

char const *
zpq_compress_error(ZpqStream * zpq)
{
	return zs_compress_error(zpq->c_stream);
}

char const *
zpq_decompress_error(ZpqStream * zpq)
{
	return zs_decompress_error(zpq->d_stream);
}

char const *
zpq_compress_algorithm_name(ZpqStream * zpq)
{
	return zs_compress_algorithm_name(zpq->c_stream);
}

char const *
zpq_decompress_algorithm_name(ZpqStream * zpq)
{
	return zs_decompress_algorithm_name(zpq->d_stream);
}

char *
zpq_algorithms(ZpqStream * zpq)
{
	return zpq_serialize_compressors(zpq->compressors, zpq->n_compressors);
}

int
zpq_parse_compression_setting(char *val, zpq_compressor * *compressors, size_t *n_compressors)
{
	int			i;
	char	  **supported_algorithms = zs_get_supported_algorithms();
	size_t		n_supported_algorithms = 0;
	char	   *protocol_extension = strchr(val, ';');

	*compressors = NULL;
	*n_compressors = 0;

	/* No protocol extensions are currently supported */
	if (protocol_extension)
		*protocol_extension = '\0';

	while (supported_algorithms[n_supported_algorithms] != NULL)
	{
		n_supported_algorithms += 1;
	}

	if (pg_strcasecmp(val, "true") == 0 ||
		pg_strcasecmp(val, "yes") == 0 ||
		pg_strcasecmp(val, "on") == 0 ||
		pg_strcasecmp(val, "any") == 0 ||
		pg_strcasecmp(val, "1") == 0)
	{
		/* return all available compressors */
		*n_compressors = n_supported_algorithms;

		if (n_supported_algorithms)
		{
			*compressors = (zpq_compressor*)malloc(n_supported_algorithms * sizeof(zpq_compressor));
			for (i = 0; i < n_supported_algorithms; i++)
			{
				(*compressors)[i].impl = i;
				(*compressors)[i].level = ZPQ_DEFAULT_COMPRESSION_LEVEL;
			}
		}
		return 1;
	}

	if (*val == 0 ||
		pg_strcasecmp(val, "false") == 0 ||
		pg_strcasecmp(val, "no") == 0 ||
		pg_strcasecmp(val, "off") == 0 ||
		pg_strcasecmp(val, "0") == 0)
	{
		/* Compression is disabled */
		return 0;
	}

	return zpq_deserialize_compressors(val, compressors, n_compressors) ? 1 : -1;
}

bool
zpq_deserialize_compressors(char const *c_string, zpq_compressor * *compressors, size_t *n_compressors)
{
	int			selected_alg_mask = 0;	/* bitmask of already selected
										 * algorithms to avoid duplicates in
										 * compressors */
	char	  **supported_algorithms = zs_get_supported_algorithms();
	size_t		n_supported_algorithms = 0;
	char	   *c_string_dup = strdup(c_string);	/* following parsing can
													 * modify the string */
	char	   *p = c_string_dup;

	*n_compressors = 0;

	while (supported_algorithms[n_supported_algorithms] != NULL)
	{
		n_supported_algorithms += 1;
	}

	*compressors = (zpq_compressor*)malloc(n_supported_algorithms * sizeof(zpq_compressor));

	while (*p != '\0')
	{
		char	   *sep = strchr(p, ',');
		char	   *col;
		int			compression_level = ZPQ_DEFAULT_COMPRESSION_LEVEL;
		bool		found;

		if (sep != NULL)
			*sep = '\0';

		col = strchr(p, ':');
		if (col != NULL)
		{
			*col = '\0';
			if (sscanf(col + 1, "%d", &compression_level) != 1)
			{
				pg_log_warning("invalid compression level %s in compression option '%s'", col + 1, p);
				free(*compressors);
				free(c_string_dup);
				*compressors = NULL;
				*n_compressors = 0;
				return false;
			}
		}
		found = false;
		for (int i = 0; supported_algorithms[i] != NULL; i++)
		{
			if (pg_strcasecmp(p, supported_algorithms[i]) == 0)
			{
				if (selected_alg_mask & (1 << i))
				{
					/* duplicates are not allowed */
					pg_log_warning("duplicate algorithm %s in compressors string %s", p, c_string);
					free(*compressors);
					free(c_string_dup);
					*compressors = NULL;
					*n_compressors = 0;
					return false;
				}

				(*compressors)[*n_compressors].impl = i;
				(*compressors)[*n_compressors].level = compression_level;

				selected_alg_mask |= 1 << i;
				*n_compressors += 1;
				found = true;
				break;
			}
		}
		if (!found)
		{
			pg_log_warning("algorithm %s is not supported", p);
		}

		if (sep)
			p = sep + 1;
		else
			break;
	}

	if (*n_compressors == 0)
	{
		free(*compressors);
		*compressors = NULL;
	}
	free(c_string_dup);
	return true;
}

char *
zpq_serialize_compressors(zpq_compressor const *compressors, size_t n_compressors)
{
	char	   *res;
	char	   *p;
	size_t		i;
	size_t		total_len = 0;
	char	  **supported_algorithms = zs_get_supported_algorithms();

	if (n_compressors == 0)
	{
		return NULL;
	}

	for (i = 0; i < n_compressors; i++)
	{
		size_t		level_len;

		if (!zs_is_valid_impl_id(compressors[i].impl))
		{
			pg_log_warning("algorithm impl_id %d is incorrect", compressors[i].impl);
			return NULL;
		}

		/* determine the length of the compression level string */
		level_len = compressors[i].level == 0 ? 1 : (int) floor(log10(abs(compressors[i].level))) + 1;
		if (compressors[i].level < 0)
		{
			level_len += 1;		/* add the leading "-" */
		}

		/*
		 * single entry looks like "alg_name:compression_level," so +2 is for
		 * ":" and "," symbols (or trailing null)
		 */
		total_len += strlen(supported_algorithms[compressors[i].impl]) + level_len + 2;
	}

	res = p = (char*)malloc(total_len);

	for (i = 0; i < n_compressors; i++)
	{
		p += sprintf(p, "%s:%d", supported_algorithms[compressors[i].impl], compressors[i].level);
		if (i < n_compressors - 1)
			*p++ = ',';
	}
	return res;
}
