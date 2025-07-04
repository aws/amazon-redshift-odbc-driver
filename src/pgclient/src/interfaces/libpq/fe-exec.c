/*-------------------------------------------------------------------------
 *
 * fe-exec.c
 *	  functions related to sending a query down to the backend
 *
 * Portions Copyright (c) 1996-2011, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/interfaces/libpq/fe-exec.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>

#include "libpq-fe.h"
#include "libpq-int.h"

#include "mb/pg_wchar.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#endif

#include "MessageLoopState.h"
#include <rslog.h>

/* keep this in same order as ExecStatusType in libpq-fe.h */
char	   *const pgresStatus[] = {
	"PGRES_EMPTY_QUERY",
	"PGRES_COMMAND_OK",
	"PGRES_TUPLES_OK",
	"PGRES_COPY_OUT",
	"PGRES_COPY_IN",
	"PGRES_BAD_RESPONSE",
	"PGRES_NONFATAL_ERROR",
	"PGRES_FATAL_ERROR",
	"PGRES_COPY_BOTH"
};

/*
 * static state needed by PQescapeBytea; initialize to
 * values that result in backward-compatible behavior
 */
static int	static_client_encoding = PG_SQL_ASCII;
static bool static_std_strings = false;


static PGEvent *dupEvents(PGEvent *events, int count);
static bool PQsendQueryStart(PGconn *conn);
static int PQsendQueryGuts(PGconn *conn,
				const char *command,
				const char *stmtName,
				int nParams,
				const Oid *paramTypes,
				const char *const * paramValues,
				const int *paramLengths,
				const int *paramFormats,
				int resultFormat);
static void parseInput(PGconn *conn);
static void _parseInput(struct _CscStatementContext *pCscStatementContext, int *piStop); // IHG
void _pqGetResultLoop(struct _CscStatementContext *pCscStatementContext); // IHG
void _pqGetResultLoopOnThread(void *_pCscStatementContext);


/* static */ bool PQexecStart(PGconn *conn);
static PGresult *PQexecFinish(PGconn *conn);
static int PQsendDescribe(PGconn *conn, char desc_type,
			   const char *desc_target);
static int	check_field_number(const PGresult *res, int field_num);

// IHG 
extern int getMaxRowsForCsc(void *pCallerContext);
extern int getResultSetTypeForCsc(void *pCallerContext);
extern int getResultConcurrencyTypeForCsc(void *pCallerContext);
void _pgFreeTuplePointers(PGresult * res);
static void pqClearForStreamingCursor(PGresult *res, PGconn *conn);
extern int isStreamingCursorMode(void *pCallerContext);

/* ----------------
 * Space management for PGresult.
 *
 * Formerly, libpq did a separate malloc() for each field of each tuple
 * returned by a query.  This was remarkably expensive --- malloc/free
 * consumed a sizable part of the application's runtime.  And there is
 * no real need to keep track of the fields separately, since they will
 * all be freed together when the PGresult is released.  So now, we grab
 * large blocks of storage from malloc and allocate space for query data
 * within these blocks, using a trivially simple allocator.  This reduces
 * the number of malloc/free calls dramatically, and it also avoids
 * fragmentation of the malloc storage arena.
 * The PGresult structure itself is still malloc'd separately.  We could
 * combine it with the first allocation block, but that would waste space
 * for the common case that no extra storage is actually needed (that is,
 * the SQL command did not return tuples).
 *
 * We also malloc the top-level array of tuple pointers separately, because
 * we need to be able to enlarge it via realloc, and our trivial space
 * allocator doesn't handle that effectively.  (Too bad the FE/BE protocol
 * doesn't tell us up front how many tuples will be returned.)
 * All other subsidiary storage for a PGresult is kept in PGresult_data blocks
 * of size PGRESULT_DATA_BLOCKSIZE.  The overhead at the start of each block
 * is just a link to the next one, if any.	Free-space management info is
 * kept in the owning PGresult.
 * A query returning a small amount of data will thus require three malloc
 * calls: one for the PGresult, one for the tuples pointer array, and one
 * PGresult_data block.
 *
 * Only the most recently allocated PGresult_data block is a candidate to
 * have more stuff added to it --- any extra space left over in older blocks
 * is wasted.  We could be smarter and search the whole chain, but the point
 * here is to be simple and fast.  Typical applications do not keep a PGresult
 * around very long anyway, so some wasted space within one is not a problem.
 *
 * Tuning constants for the space allocator are:
 * PGRESULT_DATA_BLOCKSIZE: size of a standard allocation block, in bytes
 * PGRESULT_ALIGN_BOUNDARY: assumed alignment requirement for binary data
 * PGRESULT_SEP_ALLOC_THRESHOLD: objects bigger than this are given separate
 *	 blocks, instead of being crammed into a regular allocation block.
 * Requirements for correct function are:
 * PGRESULT_ALIGN_BOUNDARY must be a multiple of the alignment requirements
 *		of all machine data types.	(Currently this is set from configure
 *		tests, so it should be OK automatically.)
 * PGRESULT_SEP_ALLOC_THRESHOLD + PGRESULT_BLOCK_OVERHEAD <=
 *			PGRESULT_DATA_BLOCKSIZE
 *		pqResultAlloc assumes an object smaller than the threshold will fit
 *		in a new block.
 * The amount of space wasted at the end of a block could be as much as
 * PGRESULT_SEP_ALLOC_THRESHOLD, so it doesn't pay to make that too large.
 * ----------------
 */

#define PGRESULT_DATA_BLOCKSIZE		2048
#define PGRESULT_ALIGN_BOUNDARY		MAXIMUM_ALIGNOF		/* from configure */
#define PGRESULT_BLOCK_OVERHEAD		Max(sizeof(PGresult_data), PGRESULT_ALIGN_BOUNDARY)
#define PGRESULT_SEP_ALLOC_THRESHOLD	(PGRESULT_DATA_BLOCKSIZE / 2)


/*
 * PQmakeEmptyPGresult
 *	 returns a newly allocated, initialized PGresult with given status.
 *	 If conn is not NULL and status indicates an error, the conn's
 *	 errorMessage is copied.  Also, any PGEvents are copied from the conn.
 */
PGresult *
PQmakeEmptyPGresult(PGconn *conn, ExecStatusType status)
{
	PGresult   *result;

	result = (PGresult *) malloc(sizeof(PGresult));
	if (!result)
		return NULL;

	result->myntups = 0;
	result->totalntups = 0;
	result->capped = 0;
	result->mem_used = 0; /* track bytes used for results */

	result->ntups = 0;
	result->numAttributes = 0;
	result->attDescs = NULL;
	result->tuples = NULL;
	result->tupArrSize = 0;
	result->numParameters = 0;
	result->paramDescs = NULL;
	result->resultStatus = status;
	result->cmdStatus[0] = '\0';
	result->binary = 0;
	result->events = NULL;
	result->nEvents = 0;
	result->errMsg = NULL;
	result->errFields = NULL;
	result->null_field[0] = '\0';
	result->curBlock = NULL;
	result->curOffset = 0;
	result->spaceLeft = 0;

    result->m_cscResult = NULL;
    result->m_tuplesAllocatedByCscRead = FALSE;

	if (conn)
	{
		/* copy connection data we might need for operations on PGresult */
		result->noticeHooks = conn->noticeHooks;
		result->client_encoding = conn->client_encoding;

		/* consider copying conn's errorMessage */
		switch (status)
		{
			case PGRES_EMPTY_QUERY:
			case PGRES_COMMAND_OK:
			case PGRES_TUPLES_OK:
			case PGRES_COPY_OUT:
			case PGRES_COPY_IN:
			case PGRES_COPY_BOTH:
				/* non-error cases */
				break;
			default:
				pqSetResultError(result, conn->errorMessage.data);
				break;
		}

		/* copy events last; result must be valid if we need to PQclear */
		if (conn->nEvents > 0)
		{
			result->events = dupEvents(conn->events, conn->nEvents);
			if (!result->events)
			{
				PQclear(result);
				return NULL;
			}
			result->nEvents = conn->nEvents;
		}
	}
	else
	{
		/* defaults... */
		result->noticeHooks.noticeRec = NULL;
		result->noticeHooks.noticeRecArg = NULL;
		result->noticeHooks.noticeProc = NULL;
		result->noticeHooks.noticeProcArg = NULL;
		result->client_encoding = PG_SQL_ASCII;
	}

	return result;
}

/*
 * PQsetResultAttrs
 *
 * Set the attributes for a given result.  This function fails if there are
 * already attributes contained in the provided result.  The call is
 * ignored if numAttributes is is zero or attDescs is NULL.  If the
 * function fails, it returns zero.  If the function succeeds, it
 * returns a non-zero value.
 */
int
PQsetResultAttrs(PGresult *res, int numAttributes, PGresAttDesc *attDescs)
{
	int			i;

	/* If attrs already exist, they cannot be overwritten. */
	if (!res || res->numAttributes > 0)
		return FALSE;

	/* ignore no-op request */
	if (numAttributes <= 0 || !attDescs)
		return TRUE;

	res->attDescs = (PGresAttDesc *)
		PQresultAlloc(res, numAttributes * sizeof(PGresAttDesc));

	if (!res->attDescs)
		return FALSE;

	res->numAttributes = numAttributes;
	memcpy(res->attDescs, attDescs, numAttributes * sizeof(PGresAttDesc));

	/* deep-copy the attribute names, and determine format */
	res->binary = 1;
	for (i = 0; i < res->numAttributes; i++)
	{
		if (res->attDescs[i].name)
			res->attDescs[i].name = pqResultStrdup(res, res->attDescs[i].name);
		else
			res->attDescs[i].name = res->null_field;

		if (!res->attDescs[i].name)
			return FALSE;

		if (res->attDescs[i].catalog_name)
			res->attDescs[i].catalog_name = pqResultStrdup(res, res->attDescs[i].catalog_name);
		else
			res->attDescs[i].catalog_name = res->null_field;

		if (res->attDescs[i].schema_name)
			res->attDescs[i].schema_name = pqResultStrdup(res, res->attDescs[i].schema_name);
		else
			res->attDescs[i].schema_name = res->null_field;

		if (res->attDescs[i].table_name)
			res->attDescs[i].table_name = pqResultStrdup(res, res->attDescs[i].table_name);
		else
			res->attDescs[i].table_name = res->null_field;

		if (res->attDescs[i].col_name)
			res->attDescs[i].col_name = pqResultStrdup(res, res->attDescs[i].col_name);
		else
			res->attDescs[i].col_name = res->null_field;

		if (res->attDescs[i].format == 0)
			res->binary = 0;
	}

	return TRUE;
}

/*
 * PQcopyResult
 *
 * Returns a deep copy of the provided 'src' PGresult, which cannot be NULL.
 * The 'flags' argument controls which portions of the result will or will
 * NOT be copied.  The created result is always put into the
 * PGRES_TUPLES_OK status.	The source result error message is not copied,
 * although cmdStatus is.
 *
 * To set custom attributes, use PQsetResultAttrs.	That function requires
 * that there are no attrs contained in the result, so to use that
 * function you cannot use the PG_COPYRES_ATTRS or PG_COPYRES_TUPLES
 * options with this function.
 *
 * Options:
 *	 PG_COPYRES_ATTRS - Copy the source result's attributes
 *
 *	 PG_COPYRES_TUPLES - Copy the source result's tuples.  This implies
 *	 copying the attrs, seeeing how the attrs are needed by the tuples.
 *
 *	 PG_COPYRES_EVENTS - Copy the source result's events.
 *
 *	 PG_COPYRES_NOTICEHOOKS - Copy the source result's notice hooks.
 */
PGresult *
PQcopyResult(const PGresult *src, int flags)
{
	PGresult   *dest;
	int			i;

	if (!src)
		return NULL;

	dest = PQmakeEmptyPGresult(NULL, PGRES_TUPLES_OK);
	if (!dest)
		return NULL;

	/* Always copy these over.	Is cmdStatus really useful here? */
	dest->client_encoding = src->client_encoding;
	strncpy(dest->cmdStatus, src->cmdStatus,sizeof(dest->cmdStatus));

	/* Wants attrs? */
	if (flags & (PG_COPYRES_ATTRS | PG_COPYRES_TUPLES))
	{
		if (!PQsetResultAttrs(dest, src->numAttributes, src->attDescs))
		{
			PQclear(dest);
			return NULL;
		}
	}

	/* Wants to copy tuples? */
	if (flags & PG_COPYRES_TUPLES)
	{
		int			tup,
					field;

		for (tup = 0; tup < src->ntups; tup++)
		{
			for (field = 0; field < src->numAttributes; field++)
			{
				if (!PQsetvalue(dest, tup, field,
								src->tuples[tup][field].value,
								src->tuples[tup][field].len))
				{
					PQclear(dest);
					return NULL;
				}
			}
		}
	}

	/* Wants to copy notice hooks? */
	if (flags & PG_COPYRES_NOTICEHOOKS)
		dest->noticeHooks = src->noticeHooks;

	/* Wants to copy PGEvents? */
	if ((flags & PG_COPYRES_EVENTS) && src->nEvents > 0)
	{
		dest->events = dupEvents(src->events, src->nEvents);
		if (!dest->events)
		{
			PQclear(dest);
			return NULL;
		}
		dest->nEvents = src->nEvents;
	}

	/* Okay, trigger PGEVT_RESULTCOPY event */
	for (i = 0; i < dest->nEvents; i++)
	{
		if (src->events[i].resultInitialized)
		{
			PGEventResultCopy evt;

			evt.src = src;
			evt.dest = dest;
			if (!dest->events[i].proc(PGEVT_RESULTCOPY, &evt,
									  dest->events[i].passThrough))
			{
				PQclear(dest);
				return NULL;
			}
			dest->events[i].resultInitialized = TRUE;
		}
	}

	return dest;
}

/*
 * Copy an array of PGEvents (with no extra space for more).
 * Does not duplicate the event instance data, sets this to NULL.
 * Also, the resultInitialized flags are all cleared.
 */
static PGEvent *
dupEvents(PGEvent *events, int count)
{
	PGEvent    *newEvents;
	int			i;

	if (!events || count <= 0)
		return NULL;

	newEvents = (PGEvent *) malloc(count * sizeof(PGEvent));
	if (!newEvents)
		return NULL;

	for (i = 0; i < count; i++)
	{
		newEvents[i].proc = events[i].proc;
		newEvents[i].passThrough = events[i].passThrough;
		newEvents[i].data = NULL;
		newEvents[i].resultInitialized = FALSE;
		newEvents[i].name = strdup(events[i].name);
		if (!newEvents[i].name)
		{
			while (--i >= 0)
				free(newEvents[i].name);
			free(newEvents);
			return NULL;
		}
	}

	return newEvents;
}


/*
 * Sets the value for a tuple field.  The tup_num must be less than or
 * equal to PQntuples(res).  If it is equal, a new tuple is created and
 * added to the result.
 * Returns a non-zero value for success and zero for failure.
 */
int
PQsetvalue(PGresult *res, int tup_num, int field_num, char *value, int len)
{
	PGresAttValue *attval;

	if (!check_field_number(res, field_num))
		return FALSE;

	/* Invalid tup_num, must be <= ntups */
	if (tup_num < 0 || tup_num > res->ntups)
		return FALSE;

	/* need to allocate a new tuple? */
	if (tup_num == res->ntups)
	{
		PGresAttValue *tup;
		int			i;

		tup = (PGresAttValue *)
			pqResultAlloc(res, res->numAttributes * sizeof(PGresAttValue),
						  TRUE);

		if (!tup)
			return FALSE;

		/* initialize each column to NULL */
		for (i = 0; i < res->numAttributes; i++)
		{
			tup[i].len = NULL_LEN;
			tup[i].value = res->null_field;
		}

		/* add it to the array */
		if (!pqAddTuple(res, tup, 0))
			return FALSE;
	}

	attval = &res->tuples[tup_num][field_num];

	/* treat either NULL_LEN or NULL value pointer as a NULL field */
	if (len == NULL_LEN || value == NULL)
	{
		attval->len = NULL_LEN;
		attval->value = res->null_field;
	}
	else if (len <= 0)
	{
		attval->len = 0;
		attval->value = res->null_field;
	}
	else
	{
		attval->value = (char *) pqResultAlloc(res, len + 1, TRUE);
		if (!attval->value)
			return FALSE;
		attval->len = len;
		memcpy(attval->value, value, len);
		attval->value[len] = '\0';
	}

	return TRUE;
}

/*
 * pqResultAlloc - exported routine to allocate local storage in a PGresult.
 *
 * We force all such allocations to be maxaligned, since we don't know
 * whether the value might be binary.
 */
void *
PQresultAlloc(PGresult *res, size_t nBytes)
{
	return pqResultAlloc(res, nBytes, TRUE);
}

/*
 * pqResultAlloc -
 *		Allocate subsidiary storage for a PGresult.
 *
 * nBytes is the amount of space needed for the object.
 * If isBinary is true, we assume that we need to align the object on
 * a machine allocation boundary.
 * If isBinary is false, we assume the object is a char string and can
 * be allocated on any byte boundary.
 */
void *
pqResultAlloc(PGresult *res, size_t nBytes, bool isBinary)
{
	char	   *space;
	PGresult_data *block;

	if (!res)
		return NULL;

	if (nBytes <= 0)
		return res->null_field;

	/*
	 * If alignment is needed, round up the current position to an alignment
	 * boundary.
	 */
	if (isBinary)
	{
		int			offset = res->curOffset % PGRESULT_ALIGN_BOUNDARY;

		if (offset)
		{
			res->curOffset += PGRESULT_ALIGN_BOUNDARY - offset;
			res->spaceLeft -= PGRESULT_ALIGN_BOUNDARY - offset;
		}
	}

	/* If there's enough space in the current block, no problem. */
	if (nBytes <= (size_t) res->spaceLeft)
	{
		space = res->curBlock->space + res->curOffset;
		res->curOffset += nBytes;
		res->spaceLeft -= nBytes;
		return space;
	}

	/*
	 * If the requested object is very large, give it its own block; this
	 * avoids wasting what might be most of the current block to start a new
	 * block.  (We'd have to special-case requests bigger than the block size
	 * anyway.)  The object is always given binary alignment in this case.
	 */
	if (nBytes >= PGRESULT_SEP_ALLOC_THRESHOLD)
	{
		block = (PGresult_data *) malloc(nBytes + PGRESULT_BLOCK_OVERHEAD);
		if (!block)
			return NULL;
		space = block->space + PGRESULT_BLOCK_OVERHEAD;
		if (res->curBlock)
		{
			/*
			 * Tuck special block below the active block, so that we don't
			 * have to waste the free space in the active block.
			 */
			block->next = res->curBlock->next;
			res->curBlock->next = block;
		}
		else
		{
			/* Must set up the new block as the first active block. */
			block->next = NULL;
			res->curBlock = block;
			res->spaceLeft = 0; /* be sure it's marked full */
		}
		return space;
	}

	/* Otherwise, start a new block. */
	block = (PGresult_data *) malloc(PGRESULT_DATA_BLOCKSIZE);
	if (!block)
		return NULL;
	block->next = res->curBlock;
	res->curBlock = block;
	if (isBinary)
	{
		/* object needs full alignment */
		res->curOffset = PGRESULT_BLOCK_OVERHEAD;
		res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - PGRESULT_BLOCK_OVERHEAD;
	}
	else
	{
		/* we can cram it right after the overhead pointer */
		res->curOffset = sizeof(PGresult_data);
		res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - sizeof(PGresult_data);
	}

	space = block->space + res->curOffset;
	res->curOffset += nBytes;
	res->spaceLeft -= nBytes;
	return space;
}

/*
 * pqResultStrdup -
 *		Like strdup, but the space is subsidiary PGresult space.
 */
char *
pqResultStrdup(PGresult *res, const char *str)
{
	int len = strlen(str) + 1;
	char	   *space = (char *) pqResultAlloc(res, len, FALSE);

	if (space)
	{
		strncpy(space, str, len);
	}
	return space;
}

/*
 * pqSetResultError -
 *		assign a new error message to a PGresult
 */
void
pqSetResultError(PGresult *res, const char *msg)
{
	if (!res)
		return;
	if (msg && *msg)
		res->errMsg = pqResultStrdup(res, msg);
	else
		res->errMsg = NULL;
}

/*
 * pqCatenateResultError -
 *		concatenate a new error message to the one already in a PGresult
 */
void
pqCatenateResultError(PGresult *res, const char *msg)
{
	PQExpBufferData errorBuf;

	if (!res || !msg)
		return;
	initPQExpBuffer(&errorBuf);
	if (res->errMsg)
		appendPQExpBufferStr(&errorBuf, res->errMsg);
	appendPQExpBufferStr(&errorBuf, msg);
	pqSetResultError(res, errorBuf.data);
	termPQExpBuffer(&errorBuf);
}

/*
 * PQclear -
 *	  free's the memory associated with a PGresult
 */
void
PQclear(PGresult *res)
{
	PGresult_data *block;
	int			i;

	if (!res)
		return;

	for (i = 0; i < res->nEvents; i++)
	{
		/* only send DESTROY to successfully-initialized event procs */
		if (res->events[i].resultInitialized)
		{
			PGEventResultDestroy evt;

			evt.result = res;
			(void) res->events[i].proc(PGEVT_RESULTDESTROY, &evt,
									   res->events[i].passThrough);
		}
		free(res->events[i].name);
	}
	res->nEvents = 0;

	if (res->events)
	{
		free(res->events);
		res->events = NULL;
	}

	/* Free all the subsidiary blocks */
	while ((block = res->curBlock) != NULL)
	{
		res->curBlock = block->next;
		free(block);
	}

	/* Free the top-level tuple pointer array */
    _pgFreeTuplePointers(res);

	/* zero out the pointer fields to catch programming errors */
	res->attDescs = NULL;
	res->tuples = NULL;
	res->paramDescs = NULL;
	res->errFields = NULL;
	res->events = NULL;
	res->nEvents = 0;

    res->m_cscResult = NULL;
    res->m_tuplesAllocatedByCscRead = FALSE;
    res->ntups = 0;

	/* res->curBlock was zeroed out earlier */

	/* Free the PGresult structure itself */
	free(res);
}

/*
 * Handy subroutine to deallocate any partially constructed async result.
 */

void
pqClearAsyncResult(PGconn *conn)
{
	if (conn->result)
		PQclear(conn->result);
	conn->result = NULL;
	conn->curTuple = NULL;
}

/*
 * This subroutine deletes any existing async result, sets conn->result
 * to a PGresult with status PGRES_FATAL_ERROR, and stores the current
 * contents of conn->errorMessage into that result.  It differs from a
 * plain call on PQmakeEmptyPGresult() in that if there is already an
 * async result with status PGRES_FATAL_ERROR, the current error message
 * is APPENDED to the old error message instead of replacing it.  This
 * behavior lets us report multiple error conditions properly, if necessary.
 * (An example where this is needed is when the backend sends an 'E' message
 * and immediately closes the connection --- we want to report both the
 * backend error and the connection closure error.)
 */
void
pqSaveErrorResult(PGconn *conn)
{
	/*
	 * If no old async result, just let PQmakeEmptyPGresult make one. Likewise
	 * if old result is not an error message.
	 */
	if (conn->result == NULL ||
		conn->result->resultStatus != PGRES_FATAL_ERROR ||
		conn->result->errMsg == NULL)
	{
		pqClearAsyncResult(conn);
		conn->result = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
	}
	else
	{
		/* Else, concatenate error message to existing async result. */
		pqCatenateResultError(conn->result, conn->errorMessage.data);
	}
}

/*
 * This subroutine prepares an async result object for return to the caller.
 * If there is not already an async result object, build an error object
 * using whatever is in conn->errorMessage.  In any case, clear the async
 * result storage and make sure PQerrorMessage will agree with the result's
 * error string.
 */
PGresult *
pqPrepareAsyncResult(PGconn *conn)
{
	PGresult   *res;

	/*
	 * conn->result is the PGresult to return.	If it is NULL (which probably
	 * shouldn't happen) we assume there is an appropriate error message in
	 * conn->errorMessage.
	 */
	res = conn->result;
	conn->result = NULL;		/* handing over ownership to caller */
	conn->curTuple = NULL;		/* just in case */
	if (!res)
		res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
	else
	{
		/*
		 * Make sure PQerrorMessage agrees with result; it could be different
		 * if we have concatenated messages.
		 */
		resetPQExpBuffer(&conn->errorMessage);
		appendPQExpBufferStr(&conn->errorMessage,
							 PQresultErrorMessage(res));
	}
	return res;
}

/*
 * pqInternalNotice - produce an internally-generated notice message
 *
 * A format string and optional arguments can be passed.  Note that we do
 * libpq_gettext() here, so callers need not.
 *
 * The supplied text is taken as primary message (ie., it should not include
 * a trailing newline, and should not be more than one line).
 */
void
pqInternalNotice(const PGNoticeHooks *hooks, const char *fmt,...)
{
	char		msgBuf[1024];
	va_list		args;
	PGresult   *res;
	int len;

	if (hooks->noticeRec == NULL)
		return;					/* nobody home to receive notice? */

	/* Format the message */
	va_start(args, fmt);
	vsnprintf(msgBuf, sizeof(msgBuf), libpq_gettext(fmt), args);
	va_end(args);
	msgBuf[sizeof(msgBuf) - 1] = '\0';	/* make real sure it's terminated */

	/* Make a PGresult to pass to the notice receiver */
	res = PQmakeEmptyPGresult(NULL, PGRES_NONFATAL_ERROR);
	if (!res)
		return;
	res->noticeHooks = *hooks;

	/*
	 * Set up fields of notice.
	 */
	pqSaveMessageField(res, PG_DIAG_MESSAGE_PRIMARY, msgBuf);
	pqSaveMessageField(res, PG_DIAG_SEVERITY, libpq_gettext("NOTICE"));
	/* XXX should provide a SQLSTATE too? */

	/*
	 * Result text is always just the primary message + newline. If we can't
	 * allocate it, don't bother invoking the receiver.
	 */
	len = strlen(msgBuf) + 2;
	res->errMsg = (char *) pqResultAlloc(res, len, FALSE);
	if (res->errMsg)
	{
		snprintf(res->errMsg,len, "%s\n", msgBuf);

		/*
		 * Pass to receiver, then free it.
		 */
		(*res->noticeHooks.noticeRec) (res->noticeHooks.noticeRecArg, res);
	}
	PQclear(res);
}

/*
 * pqAddTuple
 *	  add a row pointer to the PGresult structure, growing it if necessary
 *	  Returns TRUE if OK, FALSE if not enough memory to add the row
 */
int
pqAddTuple(PGresult *res, PGresAttValue *tup, int iStreamingCursorRows)
{
	if (res->ntups >= res->tupArrSize)
	{
		/*
		 * Try to grow the array.
		 *
		 * We can use realloc because shallow copying of the structure is
		 * okay. Note that the first time through, res->tuples is NULL. While
		 * ANSI says that realloc() should act like malloc() in that case,
		 * some old C libraries (like SunOS 4.1.x) coredump instead. On
		 * failure realloc is supposed to return NULL without damaging the
		 * existing allocation. Note that the positions beyond res->ntups are
		 * garbage, not necessarily NULL.
		 */
		int			newSize; 
		PGresAttValue **newTuples;

		if(iStreamingCursorRows > 0)
		{
			int allocBatchSize = (iStreamingCursorRows > 1000) ? 1000 : iStreamingCursorRows;

			newSize = (res->tupArrSize > 0) ? res->tupArrSize + allocBatchSize : allocBatchSize;
		}
		else
			newSize = (res->tupArrSize > 0) ? res->tupArrSize * 2 : 128;


		if (res->tuples == NULL)
			newTuples = (PGresAttValue **)
				malloc(newSize * sizeof(PGresAttValue *));
		else
			newTuples = (PGresAttValue **)
				realloc(res->tuples, newSize * sizeof(PGresAttValue *));

		if (!newTuples)
			return FALSE;		/* malloc or realloc failed */
		res->tupArrSize = newSize;
		res->tuples = newTuples;
	}
	res->tuples[res->ntups] = tup;
	res->ntups++;
	return TRUE;
}

/*
 * pqSaveMessageField - save one field of an error or notice message
 */
void
pqSaveMessageField(PGresult *res, char code, const char *value)
{
	PGMessageField *pfield;
	int len =  strlen(value) + 1;

	pfield = (PGMessageField *)
		pqResultAlloc(res,
			sizeof(PGMessageField) + len ,
					  TRUE);
	if (!pfield)
		return;					/* out of memory? */
	pfield->code = code;
	strncpy(pfield->contents, value, len);
	pfield->next = res->errFields;
	res->errFields = pfield;
}

/*
 * pqSaveParameterStatus - remember parameter status sent by backend
 */
void
pqSaveParameterStatus(PGconn *conn, const char *name, const char *value)
{
	pgParameterStatus *pstatus;
	pgParameterStatus *prev;

	if (conn->Pfdebug)
		RS_LOG_DEBUG("ODBCPQ",  "pqSaveParameterStatus: '%s' = '%s'\n",
				name, value);

	/*
	 * Forget any old information about the parameter
	 */
	for (pstatus = conn->pstatus, prev = NULL;
		 pstatus != NULL;
		 prev = pstatus, pstatus = pstatus->next)
	{
		if (strcmp(pstatus->name, name) == 0)
		{
			if (prev)
				prev->next = pstatus->next;
			else
				conn->pstatus = pstatus->next;
			free(pstatus);		/* frees name and value strings too */
			break;
		}
	}

	/*
	 * Store new info as a single malloc block
	 */
	pstatus = (pgParameterStatus *) malloc(sizeof(pgParameterStatus) +
										   strlen(name) + strlen(value) + 2);
	if (pstatus)
	{
		char	   *ptr;

		ptr = ((char *) pstatus) + sizeof(pgParameterStatus);
		pstatus->name = ptr;
		strncpy(ptr, name, strlen(name)+1);
		ptr += strlen(name) + 1;
		pstatus->value = ptr;
		strncpy(ptr, value, strlen(value)+1);
		pstatus->next = conn->pstatus;
		conn->pstatus = pstatus;
	}

	/*
	 * Special hacks: remember client_encoding and
	 * standard_conforming_strings, and convert server version to a numeric
	 * form.  We keep the first two of these in static variables as well, so
	 * that PQescapeBytea can behave somewhat sanely (at
	 * least in single-connection-using programs).
	 */
	if (strcmp(name, "client_encoding") == 0)
	{
		conn->client_encoding = pg_char_to_encoding(value);
		/* if we don't recognize the encoding name, fall back to SQL_ASCII */
		if (conn->client_encoding < 0)
			conn->client_encoding = PG_SQL_ASCII;
		static_client_encoding = conn->client_encoding;
	}
	else if (strcmp(name, "standard_conforming_strings") == 0)
	{
		conn->std_strings = (strcmp(value, "on") == 0);
		static_std_strings = conn->std_strings;
	}
	else if (strcmp(name, "server_version") == 0)
	{
		int			cnt;
		int			vmaj,
					vmin,
					vrev;

		cnt = sscanf(value, "%d.%d.%d", &vmaj, &vmin, &vrev);

		if (cnt < 2)
			conn->sversion = 0; /* unknown */
		else
		{
			if (cnt == 2)
				vrev = 0;
			conn->sversion = (100 * vmaj + vmin) * 100 + vrev;
		}
	}
	else if (strcmp(name, "server_protocol_version") == 0)
	{
		/* server response version. */
		sscanf(value, "%d", &(conn->server_protocol_version));
	}
}


/*
 * PQsendQuery
 *	 Submit a query, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendQuery(PGconn *conn, const char *query)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!query)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	/* construct the outgoing Query message */
	if (pqPutMsgStart('Q', false, conn) < 0 ||
		pqPuts(query, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
	{
		pqHandleSendFailure(conn);
		return 0;
	}

	/* remember we are using simple query protocol */
	conn->queryclass = PGQUERY_SIMPLE;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	conn->last_query = strdup(query);

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
	{
		pqHandleSendFailure(conn);
		return 0;
	}

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;
}

/*
 * PQsendQueryParams
 *		Like PQsendQuery, but use protocol 3.0 so we can pass parameters
 */
int
PQsendQueryParams(PGconn *conn,
				  const char *command,
				  int nParams,
				  const Oid *paramTypes,
				  const char *const * paramValues,
				  const int *paramLengths,
				  const int *paramFormats,
				  int resultFormat)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!command)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	return PQsendQueryGuts(conn,
						   command,
						   "",	/* use unnamed statement */
						   nParams,
						   paramTypes,
						   paramValues,
						   paramLengths,
						   paramFormats,
						   resultFormat);
}

/*
 * PQsendPrepare
 *	 Submit a Parse message, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendPrepare(PGconn *conn,
			  const char *stmtName, const char *query,
			  int nParams, const Oid *paramTypes)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!stmtName)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("statement name is a null pointer\n"));
		return 0;
	}

	if (!query)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/* construct the Parse message */
	if (pqPutMsgStart('P', false, conn) < 0 ||
		pqPuts(stmtName, conn) < 0 ||
		pqPuts(query, conn) < 0)
		goto sendFailed;

	if (nParams > 0 && paramTypes)
	{
		int			i;

		if (pqPutInt(nParams, 2, conn) < 0)
			goto sendFailed;
		for (i = 0; i < nParams; i++)
		{
			if (pqPutInt(paramTypes[i], 4, conn) < 0)
				goto sendFailed;
		}
	}
	else
	{
		if (pqPutInt(0, 2, conn) < 0)
			goto sendFailed;
	}
	if (pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are doing just a Parse */
	conn->queryclass = PGQUERY_PREPARE;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	conn->last_query = strdup(query);

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * pqSendPrepareAndDescribe
 *	 Submit a Parse message, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
pqSendPrepareAndDescribe(PGconn *conn,
			  const char *stmtName, const char *query,
			  int nParams, const Oid *paramTypes)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!stmtName)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("statement name is a null pointer\n"));
		return 0;
	}

	if (!query)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/* construct the Parse message */
	if (pqPutMsgStart('P', false, conn) < 0 ||
		pqPuts(stmtName, conn) < 0 ||
		pqPuts(query, conn) < 0)
		goto sendFailed;

	if (nParams > 0 && paramTypes)
	{
		int			i;

		if (pqPutInt(nParams, 2, conn) < 0)
			goto sendFailed;
		for (i = 0; i < nParams; i++)
		{
			if (pqPutInt(paramTypes[i], 4, conn) < 0)
				goto sendFailed;
		}
	}
	else
	{
		if (pqPutInt(0, 2, conn) < 0)
			goto sendFailed;
	}
	if (pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	// IHG....Start

	/* construct the Describe message */
	if (pqPutMsgStart('D', false, conn) < 0 ||
		pqPutc('S', conn) < 0 ||
		pqPuts(stmtName, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed; 

	// IHG ....END


	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are doing just a Parse */
	conn->queryclass = PGQUERY_PREPARE;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	conn->last_query = strdup(query);

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * PQsendQueryPrepared
 *		Like PQsendQuery, but execute a previously prepared statement,
 *		using protocol 3.0 so we can pass parameters
 */
int
PQsendQueryPrepared(PGconn *conn,
					const char *stmtName,
					int nParams,
					const char *const * paramValues,
					const int *paramLengths,
					const int *paramFormats,
					int resultFormat)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!stmtName)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("statement name is a null pointer\n"));
		return 0;
	}

	return PQsendQueryGuts(conn,
						   NULL,	/* no command to parse */
						   stmtName,
						   nParams,
						   NULL,	/* no param types */
						   paramValues,
						   paramLengths,
						   paramFormats,
						   resultFormat);
}

/*
 * Common startup code for PQsendQuery and sibling routines
 */
static bool
PQsendQueryStart(PGconn *conn)
{
	if (!conn)
		return false;

	/* clear the error string */
	resetPQExpBuffer(&conn->errorMessage);

	/* Don't try to send if we know there's no live connection. */
	if (conn->status != CONNECTION_OK)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no connection to the server\n"));
		return false;
	}
	/* Can't send while already busy, either. */
	if (conn->asyncStatus != PGASYNC_IDLE)
	{
		printfPQExpBuffer(&conn->errorMessage,
				  libpq_gettext("another command is already in progress\n"));
		return false;
	}

	/* initialize async result-accumulation state */
	conn->result = NULL;
	conn->curTuple = NULL;

	/* ready to send command message */
	return true;
}

/*
 * PQsendQueryGuts
 *		Common code for protocol-3.0 query sending
 *		PQsendQueryStart should be done already
 *
 * command may be NULL to indicate we use an already-prepared statement
 */
static int
PQsendQueryGuts(PGconn *conn,
				const char *command,
				const char *stmtName,
				int nParams,
				const Oid *paramTypes,
				const char *const * paramValues,
				const int *paramLengths,
				const int *paramFormats,
				int resultFormat)
{
	int			i;

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/*
	 * We will send Parse (if needed), Bind, Describe Portal, Execute, Sync,
	 * using specified statement name and the unnamed portal.
	 */

	if (command)
	{
		/* construct the Parse message */
		if (pqPutMsgStart('P', false, conn) < 0 ||
			pqPuts(stmtName, conn) < 0 ||
			pqPuts(command, conn) < 0)
			goto sendFailed;
		if (nParams > 0 && paramTypes)
		{
			if (pqPutInt(nParams, 2, conn) < 0)
				goto sendFailed;
			for (i = 0; i < nParams; i++)
			{
				if (pqPutInt(paramTypes[i], 4, conn) < 0)
					goto sendFailed;
			}
		}
		else
		{
			if (pqPutInt(0, 2, conn) < 0)
				goto sendFailed;
		}
		if (pqPutMsgEnd(conn) < 0)
			goto sendFailed;
	}

	/* Construct the Bind message */
	if (pqPutMsgStart('B', false, conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPuts(stmtName, conn) < 0)
		goto sendFailed;

	/* Send parameter formats */
	if (nParams > 0 && paramFormats)
	{
		if (pqPutInt(nParams, 2, conn) < 0)
			goto sendFailed;
		for (i = 0; i < nParams; i++)
		{
			if (pqPutInt(paramFormats[i], 2, conn) < 0)
				goto sendFailed;
		}
	}
	else
	{
		if (pqPutInt(0, 2, conn) < 0)
			goto sendFailed;
	}

	if (pqPutInt(nParams, 2, conn) < 0)
		goto sendFailed;

	/* Send parameters */
	for (i = 0; i < nParams; i++)
	{
		if (paramValues && paramValues[i])
		{
			int			nbytes;

			if (paramFormats && paramFormats[i] != 0)
			{
				/* binary parameter */
				if (paramLengths)
					nbytes = paramLengths[i];
				else
				{
					printfPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("length must be given for binary parameter\n"));
					goto sendFailed;
				}
			}
			else
			{
				/* text parameter, do not use paramLengths */
				nbytes = strlen(paramValues[i]);
			}
			if (pqPutInt(nbytes, 4, conn) < 0 ||
				pqPutnchar(paramValues[i], nbytes, conn) < 0)
				goto sendFailed;
		}
		else
		{
			/* take the param as NULL */
			if (pqPutInt(-1, 4, conn) < 0)
				goto sendFailed;
		}
	}
	if (pqPutInt(1, 2, conn) < 0 ||
		pqPutInt(resultFormat, 2, conn))
		goto sendFailed;
	if (pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Describe Portal message */
	if (pqPutMsgStart('D', false, conn) < 0 ||
		pqPutc('P', conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Execute message */
	if (pqPutMsgStart('E', false, conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPutInt(0, 4, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are using extended query protocol */
	conn->queryclass = PGQUERY_EXTENDED;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	if (command)
		conn->last_query = strdup(command);
	else
		conn->last_query = NULL;

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * pqHandleSendFailure: try to clean up after failure to send command.
 *
 * Primarily, what we want to accomplish here is to process an async
 * NOTICE message that the backend might have sent just before it died.
 *
 * NOTE: this routine should only be called in PGASYNC_IDLE state.
 */
void
pqHandleSendFailure(PGconn *conn)
{
	/*
	 * Accept any available input data, ignoring errors.  Note that if
	 * pqReadData decides the backend has closed the channel, it will close
	 * our side of the socket --- that's just what we want here.
	 */
	while (pqReadData(conn) > 0)
		 /* loop until no more data readable */ ;

	/*
	 * Parse any available input messages.	Since we are in PGASYNC_IDLE
	 * state, only NOTICE and NOTIFY messages will be eaten.
	 */
	parseInput(conn);
}

/*
 * Consume any available input from the backend
 * 0 return: some kind of trouble
 * 1 return: no problem
 */
int
PQconsumeInput(PGconn *conn)
{
	if (!conn)
		return 0;

	/*
	 * for non-blocking connections try to flush the send-queue, otherwise we
	 * may never get a response for something that may not have already been
	 * sent because it's in our write buffer!
	 */
	if (pqIsnonblocking(conn))
	{
		if (pqFlush(conn) < 0)
			return 0;
	}

	/*
	 * Load more data, if available. We do this no matter what state we are
	 * in, since we are probably getting called because the application wants
	 * to get rid of a read-select condition. Note that we will NOT block
	 * waiting for more input.
	 */
	if (pqReadData(conn) < 0)
		return 0;

	/* Parsing of the data waits till later. */
	return 1;
}


/*
 * parseInput: if appropriate, parse input data from backend
 * until input is exhausted or a stopping state is reached.
 * Note that this function will NOT attempt to read more data from the backend.
 */
static void
parseInput(PGconn *conn)
{
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		pqParseInput3(conn);
	else
		pqParseInput2(conn);
}

// IHG changes for ODBC.
static void
_parseInput(struct _CscStatementContext *pCscStatementContext,int *piStop)
{
    PGconn *conn = pCscStatementContext->m_pgConn;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		_pqParseInput3(pCscStatementContext,piStop);
	else
		pqParseInput2(conn);
}

/*
 * PQisBusy
 *	 Return TRUE if PQgetResult would block waiting for input.
 */

int
PQisBusy(PGconn *conn)
{
	if (!conn)
		return FALSE;

	/* Parse any available data, if our state permits. */
	parseInput(conn);

	/* PQgetResult will return immediately in all states except BUSY. */
	return conn->asyncStatus == PGASYNC_BUSY;
}


/*
 * PQgetResult
 *	  Get the next PGresult produced by a query.  Returns NULL if no
 *	  query work remains or an error has occurred (e.g. out of
 *	  memory).
 */

PGresult *
PQgetResult(PGconn *conn)
{
    return pqGetResult(conn, NULL);
}

// IHG changes for ODBC
PGresult *
pqGetResult(PGconn *conn, struct _CscStatementContext *pCscStatementContext)
{
	PGresult   *res;
    struct _CscStatementContext cscStatementContext;

	if (!conn)
		return NULL;

    if(pCscStatementContext == NULL)
    {
        memset(&cscStatementContext, '\0', sizeof(struct _CscStatementContext));
        cscStatementContext.m_pgConn = conn;
        pCscStatementContext = &cscStatementContext;
    }
    else
        pCscStatementContext->m_pgConn = conn;

    // Message loop
    _pqGetResultLoop(pCscStatementContext);

    if(pCscStatementContext->iSocketError)
    {
        pCscStatementContext->iSocketError = FALSE;
		return pqPrepareAsyncResult(conn);
    }
    else
    if(pCscStatementContext->m_cscThreadCreated)
    {
        return conn->result; // We still keep result in conn because csc thread may be using the same.
    }
    else
    if(isStreamingCursorMode(pCscStatementContext->m_pCallerContext)
		&& !isEndOfStreamingCursor(pCscStatementContext)
		&& pCscStatementContext->m_StreamingCursorInfo.m_streamResultBatchNumber > 0)
    {
        return conn->result; 
    }

	/* Return the appropriate thing. */
	switch (conn->asyncStatus)
	{
		case PGASYNC_IDLE:
			res = NULL;			/* query is complete */
			break;
		case PGASYNC_READY:
			res = pqPrepareAsyncResult(conn);
			/* Set the state back to BUSY, allowing parsing to proceed. */
			conn->asyncStatus = PGASYNC_BUSY;
			break;
		case PGASYNC_COPY_IN:
			if (conn->result && conn->result->resultStatus == PGRES_COPY_IN)
				res = pqPrepareAsyncResult(conn);
			else
				res = PQmakeEmptyPGresult(conn, PGRES_COPY_IN);
			break;
		case PGASYNC_COPY_OUT:
			if (conn->result && conn->result->resultStatus == PGRES_COPY_OUT)
				res = pqPrepareAsyncResult(conn);
			else
				res = PQmakeEmptyPGresult(conn, PGRES_COPY_OUT);
			break;
		case PGASYNC_COPY_BOTH:
			if (conn->result && conn->result->resultStatus == PGRES_COPY_BOTH)
				res = pqPrepareAsyncResult(conn);
			else
				res = PQmakeEmptyPGresult(conn, PGRES_COPY_BOTH);
			break;
		default:
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("unexpected asyncStatus: %d\n"),
							  (int) conn->asyncStatus);
			res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
			break;
	}

	if (res)
	{
		int			i;

		for (i = 0; i < res->nEvents; i++)
		{
			PGEventResultCreate evt;

			evt.conn = conn;
			evt.result = res;
			if (!res->events[i].proc(PGEVT_RESULTCREATE, &evt,
									 res->events[i].passThrough))
			{
				printfPQExpBuffer(&conn->errorMessage,
								  libpq_gettext("PGEventProc \"%s\" failed during PGEVT_RESULTCREATE event\n"),
								  res->events[i].name);
				pqSetResultError(res, conn->errorMessage.data);
				res->resultStatus = PGRES_FATAL_ERROR;
				break;
			}
			res->events[i].resultInitialized = TRUE;
		}
	}

	return res;
}

// Loop for results and return error result in case of error condition from socket.
// Actual result will be return by the caller.
void _pqGetResultLoop(struct _CscStatementContext *pCscStatementContext)
{
    if(pCscStatementContext->m_pMessageLoopState && !pCscStatementContext->m_alreadyInitMsgLoopState)
    {
        pCscStatementContext->m_alreadyInitMsgLoopState = TRUE;
        initMessageLoopStateCsc(pCscStatementContext->m_pMessageLoopState, pCscStatementContext->m_pgConn->m_pCscExecutor, 
                                    getMaxRowsForCsc(pCscStatementContext->m_pCallerContext), 
                                    getResultSetTypeForCsc(pCscStatementContext->m_pCallerContext), TRUE, 
                                    pCscStatementContext->m_pgConn->result,getResultConcurrencyTypeForCsc(pCscStatementContext->m_pCallerContext));
    }

    _pqGetResultLoopOnThread(pCscStatementContext);
}

// Do the message loop for the result on thread for CSC
void _pqGetResultLoopOnThread(void *_pCscStatementContext)
{
    struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;
    PGconn *conn = pCscStatementContext->m_pgConn;
    int    iStop = FALSE;

    pCscStatementContext->iSocketError = FALSE;

	/* Parse any available data, if our state permits. */
	_parseInput(pCscStatementContext,&iStop);

	/* If not ready to return something, block until we are. */
	while (conn->asyncStatus == PGASYNC_BUSY)
	{
		int			flushResult;

        // If CSC thread created break the loop
        if(iStop || (pCscStatementContext->m_cscThreadCreated && !(pCscStatementContext->m_calledFromCscThread)))
            break;

		/*
		 * If data remains unsent, send it.  Else we might be waiting for the
		 * result of a command the backend hasn't even got yet.
		 */
		while ((flushResult = pqFlush(conn)) > 0)
		{
			if (pqWait(FALSE, TRUE, conn))
			{
				flushResult = -1;
				break;
			}
		}

		/* Wait for some more data, and load it. */
		if (flushResult ||
			pqWait(TRUE, FALSE, conn) ||
			pqReadData(conn) < 0)
		{
			/*
			 * conn->errorMessage has been set by pqWait or pqReadData. We
			 * want to append it to any already-received error message.
			 */
			pqSaveErrorResult(conn);
			conn->asyncStatus = PGASYNC_IDLE;
            pCscStatementContext->iSocketError = TRUE;


            return;
		}


		/* Parse it. */
		_parseInput(pCscStatementContext,&iStop);
	} // BUSY loop
}

// IHG
PGresult *
pqgetResultForDescribeParam(PGconn *conn)
{
	PGresult   *res = NULL;

	if (!conn)
		return NULL;

	if(conn->resultForDescParam == conn->result)
	{
		res = conn->resultForDescParam;
		conn->resultForDescParam = NULL;
		conn->result = NULL;
	}

	return res;
}

// IHG
PGresult *
pqgetResultForDescribeRowPrep(PGconn *conn)
{
	PGresult   *res = NULL;

	if (!conn)
		return NULL;

	if(conn->resultForDescRowPrep == conn->result)
	{
		res = conn->resultForDescRowPrep;
		conn->resultForDescRowPrep = NULL;
		conn->result = NULL;
	}

	return res;
}

PGresult *
PQgetResultNbytes(PGconn *conn)
{
        PGresult   *res;

        if (!conn)
                return NULL;

        /* Parse any available data, if our state permits. */
        parseInput(conn);

        /* If not ready to return something, block until we are. */
        while (conn->asyncStatus == PGASYNC_BUSY)
        {
                int                     flushResult;

                if (conn->result && conn->output_nbytes &&
                    (conn->result->mem_used >= conn->output_nbytes))
                  break;
                /*
                 * If data remains unsent, send it.  Else we might be waiting for
                 * the result of a command the backend hasn't even got yet.
                 */
                while ((flushResult = pqFlush(conn)) > 0)
                {
                        if (pqWait(FALSE, TRUE, conn))
                        {
                                flushResult = -1;
                                break;
                        }
                }

                /* Wait for some more data, and load it. */
                if (flushResult ||
                        pqWait(TRUE, FALSE, conn) ||
                        pqReadData(conn) < 0)
                {
                        /*
                         * conn->errorMessage has been set by pqWait or pqReadData. We
                         * want to append it to any already-received error message.
                         */
                        pqSaveErrorResult(conn);
                        conn->asyncStatus = PGASYNC_IDLE;
                        return pqPrepareAsyncResult(conn);
                }

                /* Parse it. */
                parseInput(conn);
        }

        /* Return the appropriate thing. */
        switch (conn->asyncStatus)
        {
                case PGASYNC_IDLE:
                        res = NULL;                     /* query is complete */
                        break;
                case PGASYNC_BUSY:
                        res = conn->result;
                        conn->asyncStatus = PGASYNC_BUSY;
                        break;
                case PGASYNC_READY:
                        res = pqPrepareAsyncResult(conn);
                        /* Set the state back to BUSY, allowing parsing to proceed. */
                        conn->asyncStatus = PGASYNC_BUSY;
                        break;
                case PGASYNC_COPY_IN:
                        if (conn->result && conn->result->resultStatus == PGRES_COPY_IN)
                                res = pqPrepareAsyncResult(conn);
                        else
                                res = PQmakeEmptyPGresult(conn, PGRES_COPY_IN);
                        break;
                case PGASYNC_COPY_OUT:
                        if (conn->result && conn->result->resultStatus == PGRES_COPY_OUT)
                                res = pqPrepareAsyncResult(conn);
                        else
                                res = PQmakeEmptyPGresult(conn, PGRES_COPY_OUT);
                        break;
                default:
                        printfPQExpBuffer(&conn->errorMessage,
                                                   libpq_gettext("unexpected asyncStatus: %d\n"),
                                                          (int) conn->asyncStatus);
                        res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
                        break;
        }

        return res;
}


/*
 * PQexec
 *	  send a query to the backend and package up the result in a PGresult
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */
PGresult *
PQexec(PGconn *conn, const char *query)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQuery(conn, query))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * pqExec
 *	  send a query to the backend. This is for multiple results for ODBC. IHG
 *
 * Returns the first result.
 */
PGresult *
pqExec(PGconn *conn, const char *query, struct _CscStatementContext *pCscStatementContext)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQuery(conn, query))
		return NULL;
	return pqGetResult(conn, pCscStatementContext);
}

/*
 * PQexecParams
 *		Like PQexec, but use protocol 3.0 so we can pass parameters
 */
PGresult *
PQexecParams(PGconn *conn,
			 const char *command,
			 int nParams,
			 const Oid *paramTypes,
			 const char *const * paramValues,
			 const int *paramLengths,
			 const int *paramFormats,
			 int resultFormat)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryParams(conn, command,
						   nParams, paramTypes, paramValues, paramLengths,
						   paramFormats, resultFormat))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * pqexecParams
 *		Like PQexec, but use protocol 3.0 so we can pass parameters. This is for multiple results for ODBC. IHG.
 */
PGresult *
pqexecParams(PGconn *conn,
			 const char *command,
			 int nParams,
			 const Oid *paramTypes,
			 const char *const * paramValues,
			 const int *paramLengths,
			 const int *paramFormats,
			 int resultFormat,
             struct _CscStatementContext *pCscStatementContext)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryParams(conn, command,
						   nParams, paramTypes, paramValues, paramLengths,
						   paramFormats, resultFormat))
		return NULL;
	return pqGetResult(conn, pCscStatementContext);
}

/*
 * PQprepare
 *	  Creates a prepared statement by issuing a v3.0 parse message.
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */
PGresult *
PQprepare(PGconn *conn,
		  const char *stmtName, const char *query,
		  int nParams, const Oid *paramTypes)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendPrepare(conn, stmtName, query, nParams, paramTypes))
		return NULL;
	return PQexecFinish(conn);
}

// IHG: For ODBC multi result. This will return first result.
PGresult *
pqPrepare(PGconn *conn,
		  const char *stmtName, const char *query,
		  int nParams, const Oid *paramTypes)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!pqSendPrepareAndDescribe(conn, stmtName, query, nParams, paramTypes)) // PQsendPrepare
		return NULL;
	return PQgetResult(conn);
}

/*
 * PQexecPrepared
 *		Like PQexec, but execute a previously prepared statement,
 *		using protocol 3.0 so we can pass parameters
 */
PGresult *
PQexecPrepared(PGconn *conn,
			   const char *stmtName,
			   int nParams,
			   const char *const * paramValues,
			   const int *paramLengths,
			   const int *paramFormats,
			   int resultFormat)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryPrepared(conn, stmtName,
							 nParams, paramValues, paramLengths,
							 paramFormats, resultFormat))
		return NULL;
	return PQexecFinish(conn);
}

// Return first result. IHG
PGresult *
pqExecPrepared(PGconn *conn,
			   const char *stmtName,
			   int nParams,
			   const char *const * paramValues,
			   const int *paramLengths,
			   const int *paramFormats,
			   int resultFormat,
               struct _CscStatementContext *pCscStatementContext)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryPrepared(conn, stmtName,
							 nParams, paramValues, paramLengths,
							 paramFormats, resultFormat))
		return NULL;
	return pqGetResult(conn, pCscStatementContext);
}

/*
 * Common code for PQexec and sibling routines: prepare to send command
 */
/* static */ bool
PQexecStart(PGconn *conn)
{
	PGresult   *result;

	if (!conn)
		return false;

	/*
	 * Silently discard any prior query result that application didn't eat.
	 * This is probably poor design, but it's here for backward compatibility.
	 */
	while ((result = PQgetResult(conn)) != NULL)
	{
		ExecStatusType resultStatus = result->resultStatus;

		PQclear(result);		/* only need its status */
		if (resultStatus == PGRES_COPY_IN)
		{
			if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
			{
				/* In protocol 3, we can get out of a COPY IN state */
				if (PQputCopyEnd(conn,
						 libpq_gettext("COPY terminated by new PQexec")) < 0)
					return false;
				/* keep waiting to swallow the copy's failure message */
			}
			else
			{
				/* In older protocols we have to punt */
				printfPQExpBuffer(&conn->errorMessage,
				  libpq_gettext("COPY IN state must be terminated first\n"));
				return false;
			}
		}
		else if (resultStatus == PGRES_COPY_OUT)
		{
			if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
			{
				/*
				 * In protocol 3, we can get out of a COPY OUT state: we just
				 * switch back to BUSY and allow the remaining COPY data to be
				 * dropped on the floor.
				 */
				conn->asyncStatus = PGASYNC_BUSY;
				/* keep waiting to swallow the copy's completion message */
			}
			else
			{
				/* In older protocols we have to punt */
				printfPQExpBuffer(&conn->errorMessage,
				 libpq_gettext("COPY OUT state must be terminated first\n"));
				return false;
			}
		}
		else if (resultStatus == PGRES_COPY_BOTH)
		{
			/* We don't allow PQexec during COPY BOTH */
			printfPQExpBuffer(&conn->errorMessage,
					 libpq_gettext("PQexec not allowed during COPY BOTH\n"));
			return false;
		}
		/* check for loss of connection, too */
		if (conn->status == CONNECTION_BAD)
			return false;
	}

	/* OK to send a command */
	return true;
}

/*
 * Common code for PQexec and sibling routines: wait for command result
 */
static PGresult *
PQexecFinish(PGconn *conn)
{
	PGresult   *result;
	PGresult   *lastResult;

	/*
	 * For backwards compatibility, return the last result if there are more
	 * than one --- but merge error messages if we get more than one error
	 * result.
	 *
	 * We have to stop if we see copy in/out/both, however. We will resume
	 * parsing after application performs the data transfer.
	 *
	 * Also stop if the connection is lost (else we'll loop infinitely).
	 */
	lastResult = NULL;
	while ((result = PQgetResult(conn)) != NULL)
	{
		if (lastResult)
		{
			if (lastResult->resultStatus == PGRES_FATAL_ERROR &&
				result->resultStatus == PGRES_FATAL_ERROR)
			{
				pqCatenateResultError(lastResult, result->errMsg);
				PQclear(result);
				result = lastResult;

				/*
				 * Make sure PQerrorMessage agrees with concatenated result
				 */
				resetPQExpBuffer(&conn->errorMessage);
				appendPQExpBufferStr(&conn->errorMessage, result->errMsg);
			}
			else
				PQclear(lastResult);
		}
		lastResult = result;
		if (result->resultStatus == PGRES_COPY_IN ||
			result->resultStatus == PGRES_COPY_OUT ||
			result->resultStatus == PGRES_COPY_BOTH ||
			conn->status == CONNECTION_BAD)
			break;
	}

	return lastResult;
}

/*
 * PQdescribePrepared
 *	  Obtain information about a previously prepared statement
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).	On success, the PGresult contains status
 * PGRES_COMMAND_OK, and its parameter and column-heading fields describe
 * the statement's inputs and outputs respectively.
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */
PGresult *
PQdescribePrepared(PGconn *conn, const char *stmt)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendDescribe(conn, 'S', stmt))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQdescribePortal
 *	  Obtain information about a previously created portal
 *
 * This is much like PQdescribePrepared, except that no parameter info is
 * returned.  Note that at the moment, libpq doesn't really expose portals
 * to the client; but this can be used with a portal created by a SQL
 * DECLARE CURSOR command.
 */
PGresult *
PQdescribePortal(PGconn *conn, const char *portal)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendDescribe(conn, 'P', portal))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQsendDescribePrepared
 *	 Submit a Describe Statement command, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendDescribePrepared(PGconn *conn, const char *stmt)
{
	return PQsendDescribe(conn, 'S', stmt);
}

/*
 * PQsendDescribePortal
 *	 Submit a Describe Portal command, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendDescribePortal(PGconn *conn, const char *portal)
{
	return PQsendDescribe(conn, 'P', portal);
}

/*
 * PQsendDescribe
 *	 Common code to send a Describe command
 *
 * Available options for desc_type are
 *	 'S' to describe a prepared statement; or
 *	 'P' to describe a portal.
 * Returns 1 on success and 0 on failure.
 */
static int
PQsendDescribe(PGconn *conn, char desc_type, const char *desc_target)
{
	/* Treat null desc_target as empty string */
	if (!desc_target)
		desc_target = "";

	if (!PQsendQueryStart(conn))
		return 0;

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/* construct the Describe message */
	if (pqPutMsgStart('D', false, conn) < 0 ||
		pqPutc(desc_type, conn) < 0 ||
		pqPuts(desc_target, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are doing a Describe */
	conn->queryclass = PGQUERY_DESCRIBE;

	/* reset last-query string (not relevant now) */
	if (conn->last_query)
	{
		free(conn->last_query);
		conn->last_query = NULL;
	}

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * PQnotifies
 *	  returns a PGnotify* structure of the latest async notification
 * that has not yet been handled
 *
 * returns NULL, if there is currently
 * no unhandled async notification from the backend
 *
 * the CALLER is responsible for FREE'ing the structure returned
 */
PGnotify *
PQnotifies(PGconn *conn)
{
	PGnotify   *event;

	if (!conn)
		return NULL;

	/* Parse any available data to see if we can extract NOTIFY messages. */
	parseInput(conn);

	event = conn->notifyHead;
	if (event)
	{
		conn->notifyHead = event->next;
		if (!conn->notifyHead)
			conn->notifyTail = NULL;
		event->next = NULL;		/* don't let app see the internal state */
	}
	return event;
}

/*
 * PQputCopyData - send some data to the backend during COPY IN or COPY BOTH
 *
 * Returns 1 if successful, 0 if data could not be sent (only possible
 * in nonblock mode), or -1 if an error occurs.
 */
int
PQputCopyData(PGconn *conn, const char *buffer, int nbytes)
{
	if (!conn)
		return -1;
	if (conn->asyncStatus != PGASYNC_COPY_IN &&
		conn->asyncStatus != PGASYNC_COPY_BOTH)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -1;
	}

	/*
	 * Process any NOTICE or NOTIFY messages that might be pending in the
	 * input buffer.  Since the server might generate many notices during the
	 * COPY, we want to clean those out reasonably promptly to prevent
	 * indefinite expansion of the input buffer.  (Note: the actual read of
	 * input data into the input buffer happens down inside pqSendSome, but
	 * it's not authorized to get rid of the data again.)
	 */
	parseInput(conn);

	if (nbytes > 0)
	{
		/*
		 * Try to flush any previously sent data in preference to growing the
		 * output buffer.  If we can't enlarge the buffer enough to hold the
		 * data, return 0 in the nonblock case, else hard error. (For
		 * simplicity, always assume 5 bytes of overhead even in protocol 2.0
		 * case.)
		 */
		if ((conn->outBufSize - conn->outCount - 5) < nbytes)
		{
			if (pqFlush(conn) < 0)
				return -1;
			if (pqCheckOutBufferSpace(conn->outCount + 5 + (size_t) nbytes,
									  conn))
				return pqIsnonblocking(conn) ? 0 : -1;
		}
		/* Send the data (too simple to delegate to fe-protocol files) */
		if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		{
			if (pqPutMsgStart('d', false, conn) < 0 ||
				pqPutnchar(buffer, nbytes, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
		else
		{
			if (pqPutMsgStart(0, false, conn) < 0 ||
				pqPutnchar(buffer, nbytes, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}
	return 1;
}

/*
 * PQputCopyEnd - send EOF indication to the backend during COPY IN
 *
 * After calling this, use PQgetResult() to check command completion status.
 *
 * Returns 1 if successful, 0 if data could not be sent (only possible
 * in nonblock mode), or -1 if an error occurs.
 */
int
PQputCopyEnd(PGconn *conn, const char *errormsg)
{
	if (!conn)
		return -1;
	if (conn->asyncStatus != PGASYNC_COPY_IN)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -1;
	}

	/*
	 * Send the COPY END indicator.  This is simple enough that we don't
	 * bother delegating it to the fe-protocol files.
	 */
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
	{
		if (errormsg)
		{
			/* Send COPY FAIL */
			if (pqPutMsgStart('f', false, conn) < 0 ||
				pqPuts(errormsg, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
		else
		{
			/* Send COPY DONE */
			if (pqPutMsgStart('c', false, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}

		/*
		 * If we sent the COPY command in extended-query mode, we must issue a
		 * Sync as well.
		 */
		if (conn->queryclass != PGQUERY_SIMPLE)
		{
			if (pqPutMsgStart('S', false, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}
	else
	{
		if (errormsg)
		{
			/* Ooops, no way to do this in 2.0 */
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("function requires at least protocol version 3.0\n"));
			return -1;
		}
		else
		{
			/* Send old-style end-of-data marker */
			if (pqPutMsgStart(0, false, conn) < 0 ||
				pqPutnchar("\\.\n", 3, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}

	/* Return to active duty */
	conn->asyncStatus = PGASYNC_BUSY;
	resetPQExpBuffer(&conn->errorMessage);

	/* Try to flush data */
	if (pqFlush(conn) < 0)
		return -1;

	return 1;
}

/*
 * PQgetCopyData - read a row of data from the backend during COPY OUT
 * or COPY BOTH
 *
 * If successful, sets *buffer to point to a malloc'd row of data, and
 * returns row length (always > 0) as result.
 * Returns 0 if no row available yet (only possible if async is true),
 * -1 if end of copy (consult PQgetResult), or -2 if error (consult
 * PQerrorMessage).
 */
int
PQgetCopyData(PGconn *conn, char **buffer, int async)
{
	*buffer = NULL;				/* for all failure cases */
	if (!conn)
		return -2;
	if (conn->asyncStatus != PGASYNC_COPY_OUT &&
		conn->asyncStatus != PGASYNC_COPY_BOTH)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -2;
	}
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetCopyData3(conn, buffer, async);
	else
		return pqGetCopyData2(conn, buffer, async);
}

/*
 * PQgetline - gets a newline-terminated string from the backend.
 *
 * Chiefly here so that applications can use "COPY <rel> to stdout"
 * and read the output string.	Returns a null-terminated string in s.
 *
 * XXX this routine is now deprecated, because it can't handle binary data.
 * If called during a COPY BINARY we return EOF.
 *
 * PQgetline reads up to maxlen-1 characters (like fgets(3)) but strips
 * the terminating \n (like gets(3)).
 *
 * CAUTION: the caller is responsible for detecting the end-of-copy signal
 * (a line containing just "\.") when using this routine.
 *
 * RETURNS:
 *		EOF if error (eg, invalid arguments are given)
 *		0 if EOL is reached (i.e., \n has been read)
 *				(this is required for backward-compatibility -- this
 *				 routine used to always return EOF or 0, assuming that
 *				 the line ended within maxlen bytes.)
 *		1 in other cases (i.e., the buffer was filled before \n is reached)
 */
int
PQgetline(PGconn *conn, char *s, int maxlen)
{
	if (!s || maxlen <= 0)
		return EOF;
	*s = '\0';
	/* maxlen must be at least 3 to hold the \. terminator! */
	if (maxlen < 3)
		return EOF;

	if (!conn)
		return EOF;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetline3(conn, s, maxlen);
	else
		return pqGetline2(conn, s, maxlen);
}

/*
 * PQgetlineAsync - gets a COPY data row without blocking.
 *
 * This routine is for applications that want to do "COPY <rel> to stdout"
 * asynchronously, that is without blocking.  Having issued the COPY command
 * and gotten a PGRES_COPY_OUT response, the app should call PQconsumeInput
 * and this routine until the end-of-data signal is detected.  Unlike
 * PQgetline, this routine takes responsibility for detecting end-of-data.
 *
 * On each call, PQgetlineAsync will return data if a complete data row
 * is available in libpq's input buffer.  Otherwise, no data is returned
 * until the rest of the row arrives.
 *
 * If -1 is returned, the end-of-data signal has been recognized (and removed
 * from libpq's input buffer).  The caller *must* next call PQendcopy and
 * then return to normal processing.
 *
 * RETURNS:
 *	 -1    if the end-of-copy-data marker has been recognized
 *	 0	   if no data is available
 *	 >0    the number of bytes returned.
 *
 * The data returned will not extend beyond a data-row boundary.  If possible
 * a whole row will be returned at one time.  But if the buffer offered by
 * the caller is too small to hold a row sent by the backend, then a partial
 * data row will be returned.  In text mode this can be detected by testing
 * whether the last returned byte is '\n' or not.
 *
 * The returned data is *not* null-terminated.
 */

int
PQgetlineAsync(PGconn *conn, char *buffer, int bufsize)
{
	if (!conn)
		return -1;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetlineAsync3(conn, buffer, bufsize);
	else
		return pqGetlineAsync2(conn, buffer, bufsize);
}

/*
 * PQputline -- sends a string to the backend during COPY IN.
 * Returns 0 if OK, EOF if not.
 *
 * This is deprecated primarily because the return convention doesn't allow
 * caller to tell the difference between a hard error and a nonblock-mode
 * send failure.
 */
int
PQputline(PGconn *conn, const char *s)
{
	return PQputnbytes(conn, s, strlen(s));
}

/*
 * PQputnbytes -- like PQputline, but buffer need not be null-terminated.
 * Returns 0 if OK, EOF if not.
 */
int
PQputnbytes(PGconn *conn, const char *buffer, int nbytes)
{
	if (PQputCopyData(conn, buffer, nbytes) > 0)
		return 0;
	else
		return EOF;
}

/*
 * PQendcopy
 *		After completing the data transfer portion of a copy in/out,
 *		the application must call this routine to finish the command protocol.
 *
 * When using protocol 3.0 this is deprecated; it's cleaner to use PQgetResult
 * to get the transfer status.	Note however that when using 2.0 protocol,
 * recovering from a copy failure often requires a PQreset.  PQendcopy will
 * take care of that, PQgetResult won't.
 *
 * RETURNS:
 *		0 on success
 *		1 on failure
 */
int
PQendcopy(PGconn *conn)
{
	if (!conn)
		return 0;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqEndcopy3(conn);
	else
		return pqEndcopy2(conn);
}


/* ----------------
 *		PQfn -	Send a function call to the POSTGRES backend.
 *
 *		conn			: backend connection
 *		fnid			: function id
 *		result_buf		: pointer to result buffer (&int if integer)
 *		result_len		: length of return value.
 *		actual_result_len: actual length returned. (differs from result_len
 *						  for varlena structures.)
 *		result_type		: If the result is an integer, this must be 1,
 *						  otherwise this should be 0
 *		args			: pointer to an array of function arguments.
 *						  (each has length, if integer, and value/pointer)
 *		nargs			: # of arguments in args array.
 *
 * RETURNS
 *		PGresult with status = PGRES_COMMAND_OK if successful.
 *			*actual_result_len is > 0 if there is a return value, 0 if not.
 *		PGresult with status = PGRES_FATAL_ERROR if backend returns an error.
 *		NULL on communications failure.  conn->errorMessage will be set.
 * ----------------
 */

PGresult *
PQfn(PGconn *conn,
	 int fnid,
	 int *result_buf,
	 int *actual_result_len,
	 int result_is_int,
	 const PQArgBlock *args,
	 int nargs)
{
	*actual_result_len = 0;

	if (!conn)
		return NULL;

	/* clear the error string */
	resetPQExpBuffer(&conn->errorMessage);

	if (conn->sock < 0 || conn->asyncStatus != PGASYNC_IDLE ||
		conn->result != NULL)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("connection in wrong state\n"));
		return NULL;
	}

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqFunctionCall3(conn, fnid,
							   result_buf, actual_result_len,
							   result_is_int,
							   args, nargs);
	else
		return pqFunctionCall2(conn, fnid,
							   result_buf, actual_result_len,
							   result_is_int,
							   args, nargs);
}


/* ====== accessor funcs for PGresult ======== */

ExecStatusType
PQresultStatus(const PGresult *res)
{
	if (!res)
		return PGRES_FATAL_ERROR;
	return res->resultStatus;
}

char *
PQresStatus(ExecStatusType status)
{
	if (status < 0 || status >= sizeof pgresStatus / sizeof pgresStatus[0])
		return libpq_gettext("invalid ExecStatusType code");
	return pgresStatus[status];
}

char *
PQresultErrorMessage(const PGresult *res)
{
	if (!res || !res->errMsg)
		return "";
	return res->errMsg;
}

char *
PQresultErrorField(const PGresult *res, int fieldcode)
{
	PGMessageField *pfield;

	if (!res)
		return NULL;
	for (pfield = res->errFields; pfield != NULL; pfield = pfield->next)
	{
		if (pfield->code == fieldcode)
			return pfield->contents;
	}
	return NULL;
}

int
PQntuples(const PGresult *res)
{
	if (!res)
		return 0;
	return res->ntups;
}

int
PQmyntuples(const PGresult *res)
{
	if (!res)
		return 0;
	return res->myntups;
}

int
PQtotalntuples(const PGresult *res)
{
        if (!res)
                return 0;
        return res->totalntups;
}

int
PQcapped(const PGresult *res)
{
	if (!res)
		return 0;
	return res->capped;
}

int
PQnfields(const PGresult *res)
{
	if (!res)
		return 0;
	return res->numAttributes;
}

int
PQbinaryTuples(const PGresult *res)
{
	if (!res)
		return 0;
	return res->binary;
}

/*
 * Helper routines to range-check field numbers and tuple numbers.
 * Return TRUE if OK, FALSE if not
 */

static int
check_field_number(const PGresult *res, int field_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (field_num < 0 || field_num >= res->numAttributes)
	{
		pqInternalNotice(&res->noticeHooks,
						 "column number %d is out of range 0..%d",
						 field_num, res->numAttributes - 1);
		return FALSE;
	}
	return TRUE;
}

static int
check_tuple_field_number(const PGresult *res,
						 int tup_num, int field_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (tup_num < 0 || tup_num >= res->ntups)
	{
		pqInternalNotice(&res->noticeHooks,
						 "row number %d is out of range 0..%d",
						 tup_num, res->ntups - 1);
		return FALSE;
	}
	if (field_num < 0 || field_num >= res->numAttributes)
	{
		pqInternalNotice(&res->noticeHooks,
						 "column number %d is out of range 0..%d",
						 field_num, res->numAttributes - 1);
		return FALSE;
	}
	return TRUE;
}

static int
check_param_number(const PGresult *res, int param_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (param_num < 0 || param_num >= res->numParameters)
	{
		pqInternalNotice(&res->noticeHooks,
						 "parameter number %d is out of range 0..%d",
						 param_num, res->numParameters - 1);
		return FALSE;
	}

	return TRUE;
}

/*
 * returns NULL if the field_num is invalid
 */
char *
PQfname(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].name;
	else
		return NULL;
}

/*
* returns NULL if the field_num is invalid
*/
char *
PQfcatalog_name(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].catalog_name;
	else
		return NULL;
}

/*
* returns NULL if the field_num is invalid
*/
char *
PQfschema_name(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].schema_name;
	else
		return NULL;
}

/*
* returns NULL if the field_num is invalid
*/
char *
PQftable_name(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].table_name;
	else
		return NULL;
}

/*
* returns NULL if the field_num is invalid
*/
char *
PQfcol_name(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].col_name;
	else
		return NULL;
}

/*
 * PQfnumber: find column number given column name
 *
 * The column name is parsed as if it were in a SQL statement, including
 * case-folding and double-quote processing.  But note a possible gotcha:
 * downcasing in the frontend might follow different locale rules than
 * downcasing in the backend...
 *
 * Returns -1 if no match.	In the present backend it is also possible
 * to have multiple matches, in which case the first one is found.
 */
int
PQfnumber(const PGresult *res, const char *field_name)
{
	char	   *field_case;
	bool		in_quotes;
	char	   *iptr;
	char	   *optr;
	int			i;

	if (!res)
		return -1;

	/*
	 * Note: it is correct to reject a zero-length input string; the proper
	 * input to match a zero-length field name would be "".
	 */
	if (field_name == NULL ||
		field_name[0] == '\0' ||
		res->attDescs == NULL)
		return -1;

	/*
	 * Note: this code will not reject partially quoted strings, eg
	 * foo"BAR"foo will become fooBARfoo when it probably ought to be an error
	 * condition.
	 */
	field_case = strdup(field_name);
	if (field_case == NULL)
		return -1;				/* grotty */

	in_quotes = false;
	optr = field_case;
	for (iptr = field_case; *iptr; iptr++)
	{
		char		c = *iptr;

		if (in_quotes)
		{
			if (c == '"')
			{
				if (iptr[1] == '"')
				{
					/* doubled quotes become a single quote */
					*optr++ = '"';
					iptr++;
				}
				else
					in_quotes = false;
			}
			else
				*optr++ = c;
		}
		else if (c == '"')
			in_quotes = true;
		else
		{
			c = pg_tolower((unsigned char) c);
			*optr++ = c;
		}
	}
	*optr = '\0';

	for (i = 0; i < res->numAttributes; i++)
	{
		if (strcmp(field_case, res->attDescs[i].name) == 0)
		{
			free(field_case);
			return i;
		}
	}
	free(field_case);
	return -1;
}

Oid
PQftable(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return InvalidOid;
	if (res->attDescs)
		return res->attDescs[field_num].tableid;
	else
		return InvalidOid;
}

int
PQftablecol(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].columnid;
	else
		return 0;
}

int
PQfnullable(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 2; // Unknown
	if (res->attDescs)
		return res->attDescs[field_num].nullable;
	else
		return 2; // Unknown
}

int
PQfcase_sensitive(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return -1; // Not set
	if (res->attDescs)
		return res->attDescs[field_num].case_sensitive;
	else
		return -1; // Not set
}

int
PQfauto_increment(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0; // false
	if (res->attDescs)
		return res->attDescs[field_num].autoincrement;
	else
		return 0; // false
}

int
PQfformat(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].format;
	else
		return 0;
}

Oid
PQftype(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return InvalidOid;
	if (res->attDescs)
		return res->attDescs[field_num].typid;
	else
		return InvalidOid;
}

int
PQfsize(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].typlen;
	else
		return 0;
}

int
PQfmod(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].atttypmod;
	else
		return 0;
}

char *
PQcmdStatus(PGresult *res)
{
	if (!res)
		return NULL;
	return res->cmdStatus;
}

/*
 * PQoidStatus -
 *	if the last command was an INSERT, return the oid string
 *	if not, return ""
 */
char *
PQoidStatus(const PGresult *res)
{
	/*
	 * This must be enough to hold the result. Don't laugh, this is better
	 * than what this function used to do.
	 */
	static char buf[24];

	size_t		len;

	if (!res || !res->cmdStatus || strncmp(res->cmdStatus, "INSERT ", 7) != 0)
		return "";

	len = strspn(res->cmdStatus + 7, "0123456789");
	if (len > 23)
		len = 23;
	strncpy(buf, res->cmdStatus + 7, len);
	buf[len] = '\0';

	return buf;
}

/*
 * PQoidValue -
 *	a perhaps preferable form of the above which just returns
 *	an Oid type
 */
Oid
PQoidValue(const PGresult *res)
{
	char	   *endptr = NULL;
	unsigned long result;

	if (!res ||
		!res->cmdStatus ||
		strncmp(res->cmdStatus, "INSERT ", 7) != 0 ||
		res->cmdStatus[7] < '0' ||
		res->cmdStatus[7] > '9')
		return InvalidOid;

	result = strtoul(res->cmdStatus + 7, &endptr, 10);

	if (!endptr || (*endptr != ' ' && *endptr != '\0'))
		return InvalidOid;
	else
		return (Oid) result;
}


/*
 * PQcmdTuples -
 *	If the last command was INSERT/UPDATE/DELETE/MOVE/FETCH/COPY, return
 *	a string containing the number of inserted/affected tuples. If not,
 *	return "".
 *
 *	XXX: this should probably return an int
 */
char *
PQcmdTuples(PGresult *res)
{
	char	   *p,
			   *c;

	if (!res)
		return "";

	if (strncmp(res->cmdStatus, "INSERT ", 7) == 0)
	{
		p = res->cmdStatus + 7;
		/* INSERT: skip oid and space */
		while (*p && *p != ' ')
			p++;
		if (*p == 0)
			goto interpret_error;		/* no space? */
		p++;
	}
	else if (strncmp(res->cmdStatus, "SELECT ", 7) == 0 ||
			 strncmp(res->cmdStatus, "DELETE ", 7) == 0 ||
			 strncmp(res->cmdStatus, "UPDATE ", 7) == 0)
		p = res->cmdStatus + 7;
	else if (strncmp(res->cmdStatus, "FETCH ", 6) == 0)
		p = res->cmdStatus + 6;
	else if (strncmp(res->cmdStatus, "MOVE ", 5) == 0 ||
			 strncmp(res->cmdStatus, "COPY ", 5) == 0 || 
			 strncmp(res->cmdStatus, "CTAS ", 5) == 0)
		p = res->cmdStatus + 5;
	else
		return "";

	/* check that we have an integer (at least one digit, nothing else) */
	for (c = p; *c; c++)
	{
		if (!isdigit((unsigned char) *c))
			goto interpret_error;
	}
	if (c == p)
		goto interpret_error;

	return p;

interpret_error:
	pqInternalNotice(&res->noticeHooks,
					 "could not interpret result from server: %s",
					 res->cmdStatus);
	return "";
}

/*
 * PQgetvalue:
 *	return the value of field 'field_num' of row 'tup_num'
 */
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return NULL;
	return res->tuples[tup_num][field_num].value;
}


/*
 * PQsetNumAttributes:
 *	set the number of attributes based on column number
 */
void
PQsetNumAttributes(PGresult *res, short columnNum)
{
	res->numAttributes = columnNum;
}

/**
 * @brief Creates an array of custom PGresAttDesc structures with allocated memory
 *
 * This function creates and initializes an array of PGresAttDesc structures,
 * allocating memory for the array and for each column's name field.
 *
 * @param colName Array of column names to be copied into the structures
 * @param colNum Number of columns to be created
 * @param colDatatype Array of data types corresponding to each column
 *
 * @return PGresAttDesc* Pointer to the allocated array of PGresAttDesc structures
 *         Returns NULL if memory allocation fails
 *
 * @note Caller is responsible for freeing the memory using PQcleanupCustomizeAttrs
 */
PGresAttDesc *PQcreateCustomizeAttrs(char **colName, int colNum,
                                     int *colDatatype) {
    PGresAttDesc *column =
        (PGresAttDesc *)malloc(colNum * sizeof(PGresAttDesc));
    int i;
    for (i = 0; i < colNum; i++) {
        column[i].name = malloc(128 * sizeof(char));
        strncpy(column[i].name, colName[i], 128);
        column[i].catalog_name = "";
        column[i].schema_name = "";
        column[i].table_name = "";
        column[i].col_name = "";
        column[i].typid = colDatatype[i];
        column[i].format = 0;
    }
    return column;
}

/**
 * @brief Frees memory allocated for PGresAttDesc structures
 *
 * This function performs a deep cleanup of the memory allocated by PQcreateCustomizeAttrs,
 * freeing both the individual name fields and the main array.
 *
 * @param column Pointer to the array of PGresAttDesc structures to be freed
 * @param colNum Number of columns in the array
 *
 * @note After calling this function, the caller should set their pointer to NULL
 * @note This function safely handles NULL input by returning without action
 */
void PQcleanupCustomizeAttrs(PGresAttDesc *column, int colNum) {
    if (column == NULL) {
        return;
    }

    // Free individual name allocations for each column
    for (int i = 0; i < colNum; i++) {
        free(column[i].name);
    }

    // Free the main column array
    free(column);
}

/*
 * getPQResultAttrs:
 *	return column name
 */
char* 
getPQResultAttrs(PGresult *res, int i){
	return res->attDescs[i].name;
}

/*
 * getPQResultAttrsType:
 *	return column data type oid
 */
Oid
getPQResultAttrsType(PGresult *res, int i){
	return res->attDescs[i].typid;
}

/* PQgetlength:
 *	returns the actual length of a field value in bytes.
 */
int
PQgetlength(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return 0;
	if (res->tuples[tup_num][field_num].len != NULL_LEN)
		return res->tuples[tup_num][field_num].len;
	else
		return 0;
}

/* PQgetisnull:
 *	returns the null status of a field value.
 */
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return 1;				/* pretend it is null */
	if (res->tuples[tup_num][field_num].len == NULL_LEN)
		return 1;
	else
		return 0;
}

/* PQnparams:
 *	returns the number of input parameters of a prepared statement.
 */
int
PQnparams(const PGresult *res)
{
	if (!res)
		return 0;
	return res->numParameters;
}

/* PQparamtype:
 *	returns type Oid of the specified statement parameter.
 */
Oid
PQparamtype(const PGresult *res, int param_num)
{
	if (!check_param_number(res, param_num))
		return InvalidOid;
	if (res->paramDescs)
		return res->paramDescs[param_num].typid;
	else
		return InvalidOid;
}


/* PQsetnonblocking:
 *	sets the PGconn's database connection non-blocking if the arg is TRUE
 *	or makes it blocking if the arg is FALSE, this will not protect
 *	you from PQexec(), you'll only be safe when using the non-blocking API.
 *	Needs to be called only on a connected database connection.
 */
int
PQsetnonblocking(PGconn *conn, int arg)
{
	bool		barg;

	if (!conn || conn->status == CONNECTION_BAD)
		return -1;

	barg = (arg ? TRUE : FALSE);

	/* early out if the socket is already in the state requested */
	if (barg == conn->nonblocking)
		return 0;

	/*
	 * to guarantee constancy for flushing/query/result-polling behavior we
	 * need to flush the send queue at this point in order to guarantee proper
	 * behavior. this is ok because either they are making a transition _from_
	 * or _to_ blocking mode, either way we can block them.
	 */
	/* if we are going from blocking to non-blocking flush here */
	if (pqFlush(conn))
		return -1;

	conn->nonblocking = barg;

	return 0;
}

/*
 * return the blocking status of the database connection
 *		TRUE == nonblocking, FALSE == blocking
 */
int
PQisnonblocking(const PGconn *conn)
{
	return pqIsnonblocking(conn);
}

/* libpq is thread-safe? */
int
PQisthreadsafe(void)
{
#ifdef ENABLE_THREAD_SAFETY
	return true;
#else
	return false;
#endif
}


/* try to force data out, really only useful for non-blocking users */
int
PQflush(PGconn *conn)
{
	return pqFlush(conn);
}

int
PQreadPending(PGconn *conn)
{
	return pqReadPending(conn);
}

/*
 *		PQfreemem - safely frees memory allocated
 *
 * Needed mostly by Win32, unless multithreaded DLL (/MD in VC6)
 * Used for freeing memory from PQescapeByte()a/PQunescapeBytea()
 */
void
PQfreemem(void *ptr)
{
	free(ptr);
}

/*
 * PQfreeNotify - free's the memory associated with a PGnotify
 *
 * This function is here only for binary backward compatibility.
 * New code should use PQfreemem().  A macro will automatically map
 * calls to PQfreemem.	It should be removed in the future.  bjm 2003-03-24
 */

#undef PQfreeNotify
void		PQfreeNotify(PGnotify *notify);

void
PQfreeNotify(PGnotify *notify)
{
	PQfreemem(notify);
}

/* HEX encoding support for bytea */
static const char hextbl[] = "0123456789abcdef";

static const int8 hexlookup[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static inline char
get_hex(char c)
{
	int			res = -1;

	if (c > 0 && c < 127)
		res = hexlookup[(unsigned char) c];

	return (char) res;
}


/*
 *		PQescapeBytea	- converts from binary string to the
 *		minimal encoding necessary to include the string in an SQL
 *		INSERT statement with a bytea type column as the target.
 *
 *		We can use either hex or escape (traditional) encoding.
 *		In escape mode, the following transformations are applied:
 *		'\0' == ASCII  0 == \000
 *		'\'' == ASCII 39 == ''
 *		'\\' == ASCII 92 == \\
 *		anything < 0x20, or > 0x7e ---> \ooo
 *										(where ooo is an octal expression)
 *
 *		If not std_strings, all backslashes sent to the output are doubled.
 */
static unsigned char *
PQescapeByteaInternal(PGconn *conn,
					  const unsigned char *from, size_t from_length,
					  size_t *to_length, bool std_strings, bool use_hex)
{
	const unsigned char *vp;
	unsigned char *rp;
	unsigned char *result;
	size_t		i;
	size_t		len;
	size_t		bslash_len = (std_strings ? 1 : 2);

	/*
	 * empty string has 1 char ('\0')
	 */
	len = 1;

	if (use_hex)
	{
		len += bslash_len + 1 + 2 * from_length;
	}
	else
	{
		vp = from;
		for (i = from_length; i > 0; i--, vp++)
		{
			if (*vp < 0x20 || *vp > 0x7e)
				len += bslash_len + 3;
			else if (*vp == '\'')
				len += 2;
			else if (*vp == '\\')
				len += bslash_len + bslash_len;
			else
				len++;
		}
	}

	*to_length = len;
	rp = result = (unsigned char *) malloc(len);
	if (rp == NULL)
	{
		if (conn)
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("out of memory\n"));
		return NULL;
	}

	if (use_hex)
	{
		if (!std_strings)
			*rp++ = '\\';
		*rp++ = '\\';
		*rp++ = 'x';
	}

	vp = from;
	for (i = from_length; i > 0; i--, vp++)
	{
		unsigned char c = *vp;

		if (use_hex)
		{
			*rp++ = hextbl[(c >> 4) & 0xF];
			*rp++ = hextbl[c & 0xF];
		}
		else if (c < 0x20 || c > 0x7e)
		{
			if (!std_strings)
				*rp++ = '\\';
			*rp++ = '\\';
			*rp++ = (c >> 6) + '0';
			*rp++ = ((c >> 3) & 07) + '0';
			*rp++ = (c & 07) + '0';
		}
		else if (c == '\'')
		{
			*rp++ = '\'';
			*rp++ = '\'';
		}
		else if (c == '\\')
		{
			if (!std_strings)
			{
				*rp++ = '\\';
				*rp++ = '\\';
			}
			*rp++ = '\\';
			*rp++ = '\\';
		}
		else
			*rp++ = c;
	}
	*rp = '\0';

	return result;
}

unsigned char *
PQescapeByteaConn(PGconn *conn,
				  const unsigned char *from, size_t from_length,
				  size_t *to_length)
{
	if (!conn)
		return NULL;
	return PQescapeByteaInternal(conn, from, from_length, to_length,
								 conn->std_strings,
								 (conn->sversion >= 90000));
}

unsigned char *
PQescapeBytea(const unsigned char *from, size_t from_length, size_t *to_length)
{
	return PQescapeByteaInternal(NULL, from, from_length, to_length,
								 static_std_strings,
								 false /* can't use hex */ );
}


#define ISFIRSTOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '3')
#define ISOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '7')
#define OCTVAL(CH) ((CH) - '0')

/*
 *		PQunescapeBytea - converts the null terminated string representation
 *		of a bytea, strtext, into binary, filling a buffer. It returns a
 *		pointer to the buffer (or NULL on error), and the size of the
 *		buffer in retbuflen. The pointer may subsequently be used as an
 *		argument to the function PQfreemem.
 *
 *		The following transformations are made:
 *		\\	 == ASCII 92 == \
 *		\ooo == a byte whose value = ooo (ooo is an octal number)
 *		\x	 == x (x is any character not matched by the above transformations)
 */
unsigned char *
PQunescapeBytea(const unsigned char *strtext, size_t *retbuflen)
{
	size_t		strtextlen,
				buflen;
	unsigned char *buffer,
			   *tmpbuf;
	size_t		i,
				j;

	if (strtext == NULL)
		return NULL;

	strtextlen = strlen((const char *) strtext);

	if (strtext[0] == '\\' && strtext[1] == 'x')
	{
		const unsigned char *s;
		unsigned char *p;

		buflen = (strtextlen - 2) / 2;
		/* Avoid unportable malloc(0) */
		buffer = (unsigned char *) malloc(buflen > 0 ? buflen : 1);
		if (buffer == NULL)
			return NULL;

		s = strtext + 2;
		p = buffer;
		while (*s)
		{
			char		v1,
						v2;

			/*
			 * Bad input is silently ignored.  Note that this includes
			 * whitespace between hex pairs, which is allowed by byteain.
			 */
			v1 = get_hex(*s++);
			if (!*s || v1 == (char) -1)
				continue;
			v2 = get_hex(*s++);
			if (v2 != (char) -1)
				*p++ = (v1 << 4) | v2;
		}

		buflen = p - buffer;
	}
	else
	{
		/*
		 * Length of input is max length of output, but add one to avoid
		 * unportable malloc(0) if input is zero-length.
		 */
		buffer = (unsigned char *) malloc(strtextlen + 1);
		if (buffer == NULL)
			return NULL;

		for (i = j = 0; i < strtextlen;)
		{
			switch (strtext[i])
			{
				case '\\':
					i++;
					if (strtext[i] == '\\')
						buffer[j++] = strtext[i++];
					else
					{
						if ((ISFIRSTOCTDIGIT(strtext[i])) &&
							(ISOCTDIGIT(strtext[i + 1])) &&
							(ISOCTDIGIT(strtext[i + 2])))
						{
							int			byte;

							byte = OCTVAL(strtext[i++]);
							byte = (byte << 3) + OCTVAL(strtext[i++]);
							byte = (byte << 3) + OCTVAL(strtext[i++]);
							buffer[j++] = byte;
						}
					}

					/*
					 * Note: if we see '\' followed by something that isn't a
					 * recognized escape sequence, we loop around having done
					 * nothing except advance i.  Therefore the something will
					 * be emitted as ordinary data on the next cycle. Corner
					 * case: '\' at end of string will just be discarded.
					 */
					break;

				default:
					buffer[j++] = strtext[i++];
					break;
			}
		}
		buflen = j;				/* buflen is the length of the dequoted data */
	}

	/* Shrink the buffer to be no larger than necessary */
	/* +1 avoids unportable behavior when buflen==0 */
	tmpbuf = realloc(buffer, buflen + 1);

	/* It would only be a very brain-dead realloc that could fail, but... */
	if (!tmpbuf)
	{
		free(buffer);
		return NULL;
	}

	*retbuflen = buflen;
	return tmpbuf;
}

// IHG
PGresAttValue **pqGetTuples(PGresult * res)
{
	if (!res)
		return NULL;			

    return res->tuples;
}

// IHG
void _pgFreeTuplePointers(PGresult * res)
{
    if(res)
    {
	    /* Free the top-level tuple pointer array */
	    if (res->tuples)
        {
            if(res->m_tuplesAllocatedByCscRead)
            {
                int row, col;

                res->m_tuplesAllocatedByCscRead = FALSE;

                // Rows loop
                for(row = 0; row < res->ntups;row++)
                {
                    // Columns loop
                    for(col = 0; col < res->numAttributes; col++)
                    {
                        if(res->tuples[row][col].value)
                            res->tuples[row][col].value = rs_free(res->tuples[row][col].value);
                    }

                    res->tuples[row] = rs_free(res->tuples[row]);
                }

                res->tuples = rs_free(res->tuples);
            }
            else 
		        free(res->tuples);

            res->tuples = NULL;
            res->ntups = 0;
        }
    }
}

// IHG
static void pqClearForStreamingCursor(PGresult *res, PGconn *conn)
{
	PGresult_data *block;
	int			i;

	if (!res)
		return;

	for (i = 0; i < res->nEvents; i++)
	{
		/* only send DESTROY to successfully-initialized event procs */
		if (res->events[i].resultInitialized)
		{
			PGEventResultDestroy evt;

			evt.result = res;
			(void) res->events[i].proc(PGEVT_RESULTDESTROY, &evt,
									   res->events[i].passThrough);
		}
		free(res->events[i].name);
	}
	res->nEvents = 0;

	if (res->events)
	{
		free(res->events);
		res->events = NULL;
	}

	/* Free all the subsidiary blocks */
	while ((block = res->curBlock) != NULL)
	{
		res->curBlock = block->next;
		free(block);
	}
	res->curBlock=NULL;

	/* Free the top-level tuple pointer array */
    _pgFreeTuplePointers(res);

	/* zero out the pointer fields to catch programming errors */
	res->attDescs = NULL; // TODO: Don't release it.
	res->tuples = NULL;
	res->paramDescs = NULL;
	res->errFields = NULL;
	res->events = NULL;
	res->nEvents = 0;

    res->m_cscResult = NULL;
    res->m_tuplesAllocatedByCscRead = FALSE;
    res->ntups = 0;

	/* res->curBlock was zeroed out earlier */

	res->curOffset = 0;
	res->spaceLeft = 0;
	res->tupArrSize = 0;
	res->myntups = 0;
	res->totalntups = 0;
	res->capped = 0;
	res->mem_used = 0; /* track bytes used for results */
	res->errMsg = NULL;

	if(conn)
		conn->curTuple = NULL;
}

// IHG
void pqReadNextBatchOfStreamingRows(void *_pCscStatementContext, PGresult *pgResult, PGconn *conn,int *piError)
{
	// Clear previous result rows
	pqClearForStreamingCursor(pgResult, conn);

	// Read another set of rows
	_pqGetResultLoopOnThread(_pCscStatementContext);

    if(((struct _CscStatementContext *)_pCscStatementContext)->iSocketError)
    {
        ((struct _CscStatementContext *)_pCscStatementContext)->iSocketError = FALSE;
		if(piError)
			*piError = TRUE;
    }
}

// IHG
short pqReadNextResultOfStreamingCursor(void *_pCscStatementContext, PGconn *conn)
{
	PGresult *pgResult;
	short rc;
	
	// Reset End Of Streaming Cursor
	setEndOfStreamingCursor(_pCscStatementContext,FALSE);

	// Reset stream batch number
	resetStreamingCursorBatchNumber(_pCscStatementContext);

	/* Set the state back to BUSY, allowing parsing to proceed. */
    conn->asyncStatus = PGASYNC_BUSY;

	// Already release the previous result
	conn->result = NULL;

	pgResult = pqGetResult(conn, _pCscStatementContext);

	if(pgResult)
		rc = (*((struct _CscStatementContext *)_pCscStatementContext)->m_pResultHandlerCallbackFunc)(((struct _CscStatementContext *)_pCscStatementContext)->m_pCallerContext, pgResult);
	else
		rc = 0;

   return rc;
}

// IHG
void pqResetConnectionResult(PGconn *conn)
{
	// Already release the result
	if(conn)
		conn->result = NULL;
}

// IHG
int pqSkipCurrentResultOfStreamingCursor(void *_pCscStatementContext, PGresult *pgResult, PGconn *conn)
{
	struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;
	int iSocketError = 0;

	if(pCscStatementContext && !isEndOfStreamingCursor(_pCscStatementContext))
	{
		if(pgResult)
		{
			// Set Skip result
			setSkipResultOfStreamingCursor(_pCscStatementContext, TRUE);

			// Read rest of result and skip

			// Clear previous result rows
			pqClearForStreamingCursor(pgResult, conn);

			if(conn->result)
				conn->result = NULL;

			// Read another set of rows
			_pqGetResultLoopOnThread(_pCscStatementContext);

			if(((struct _CscStatementContext *)_pCscStatementContext)->iSocketError)
			{
				((struct _CscStatementContext *)_pCscStatementContext)->iSocketError = FALSE;
				iSocketError = 1; // So caller should set pgResult as NULL
			}

			// Reset Skip result
			setSkipResultOfStreamingCursor(_pCscStatementContext, FALSE);

			// Set EOS cursor
			setEndOfStreamingCursor(_pCscStatementContext, TRUE);

			// Reset batch counter
			resetStreamingCursorBatchNumber(_pCscStatementContext);
		}
	}

	return iSocketError;
}

// IHG
void pqSkipAllResultsOfStreamingCursor(void *_pCscStatementContext, PGconn *conn)
{
	struct _CscStatementContext *pCscStatementContext = (struct _CscStatementContext *)_pCscStatementContext;

	if(pCscStatementContext && !isEndOfStreamingCursorQuery(_pCscStatementContext))
	{
		do
		{
			// Set Skip result
			setSkipResultOfStreamingCursor(_pCscStatementContext, TRUE);

			// Read rest of result and skip
			// Set connection result as NULL
			if(conn->result)
				conn->result = NULL;


			// Read another set of rows
			_pqGetResultLoopOnThread(_pCscStatementContext);

			if(((struct _CscStatementContext *)_pCscStatementContext)->iSocketError)
			{
				((struct _CscStatementContext *)_pCscStatementContext)->iSocketError = FALSE;
				setEndOfStreamingCursorQuery(_pCscStatementContext, TRUE);
			}

			// Reset Skip result
			setSkipResultOfStreamingCursor(_pCscStatementContext, FALSE);

			// Set EOS cursor
			setEndOfStreamingCursor(_pCscStatementContext, TRUE);

			// Reset batch counter
			resetStreamingCursorBatchNumber(_pCscStatementContext);

			if(!isEndOfStreamingCursorQuery(_pCscStatementContext))
			{
				// Set status busy to get next result or 'Z'
				conn->asyncStatus = PGASYNC_BUSY;
			}

			// Set connection result as NULL
			if(conn->result)
			{
				PQclear(conn->result);
				conn->result = NULL;
			}

		}while(!isEndOfStreamingCursorQuery(_pCscStatementContext));
	}
}

/*
 * pqIsIdle state?
 *	 Return TRUE if it's in idle state FALSE otherwise
 */
int
pqIsIdle(PGconn *conn)
{
	if (!conn)
		return TRUE;

	return conn->asyncStatus == PGASYNC_IDLE;
}
