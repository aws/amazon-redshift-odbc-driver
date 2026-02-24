/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rsdesc.h"
#include "rsmin.h"
#include "rsunicode.h"


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLCopyDesc copies descriptor information from one descriptor handle to another.
//
SQLRETURN  SQL_API SQLCopyDesc(SQLHDESC phdescSrc,
                               SQLHDESC phdescDest)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCopyDesc(FUNC_CALL, 0, phdescSrc, phdescDest);

    rc = RsDesc::RS_SQLCopyDesc(phdescSrc, phdescDest, FALSE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLCopyDesc(FUNC_RETURN, rc, phdescSrc, phdescDest);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Helper function for SQLCopyDesc.
//
SQLRETURN  SQL_API RsDesc::RS_SQLCopyDesc(SQLHDESC phdescSrc,
                                    SQLHDESC phdescDest,
                                    int iInternal)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDescSrc = (RS_DESC_INFO *)phdescSrc;
    RS_DESC_INFO *pDescDest = (RS_DESC_INFO *)phdescDest;
    RS_DESC_REC *pDescRecSrc;
    RS_DESC_REC *pDescRecDest;
    
    RS_DESC_HEADER& pDescHeaderSrc = pDescSrc->pDescHeader;
    RS_DESC_HEADER& pDescHeaderDest = pDescDest->pDescHeader;
    short hAllocType = pDescHeaderDest.hAllocType;

    if(!VALID_HDESC(phdescSrc)
        || !VALID_HDESC(phdescDest))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    if(!iInternal)
    {
        // Clear error list
        pDescSrc->pErrorList = clearErrorList(pDescSrc->pErrorList);
        pDescDest->pErrorList = clearErrorList(pDescDest->pErrorList);

        if(pDescDest->iType == RS_IRD
            || pDescDest->iType == RS_IPD)
        {
            rc = SQL_ERROR;
            addError(&pDescDest->pErrorList,"HY016", "Cannot modify an implementation descriptor", 0, NULL);
            goto error;
        }
    }

    // Copy header info
    if (pDescHeaderDest.valid && pDescHeaderSrc.valid) {
        pDescHeaderDest = pDescHeaderSrc;
        pDescHeaderDest.hAllocType = hAllocType;
    }

    // Release destination recs
    releaseDescriptorRecs(pDescDest);
    pDescDest->iRecListType = RS_DESC_RECS_LINKED_LIST;

    if(pDescDest->iType == RS_UNKNOWN_DESC_TYPE)
        pDescDest->iType = pDescSrc->iType;

    // Copy record(s) info
    pDescRecSrc = pDescSrc->pDescRecHead;

    while(pDescRecSrc != NULL)
    {
        RS_DESC_REC *next = pDescRecSrc->pNext;

        pDescRecDest = (RS_DESC_REC *)rs_calloc(1, sizeof(RS_DESC_REC));

        if(pDescRecDest == NULL)
        {
            rc = SQL_ERROR;
            addError(&pDescDest->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }

        // Copy rec
        memcpy(pDescRecDest, pDescRecSrc, sizeof(RS_DESC_REC));
        pDescRecDest->pDataAtExec = NULL;
        pDescRecDest->pNext = NULL;

        // Add in the list
        addDescriptorRec(pDescDest, pDescRecDest, FALSE);

        pDescRecSrc = next;
    }

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetDescField returns the current setting or value of a single field of a descriptor record.
//
SQLRETURN  SQL_API SQLGetDescField(SQLHDESC phdesc,
                                   SQLSMALLINT hRecNumber, 
                                   SQLSMALLINT hFieldIdentifier,
                                   SQLPOINTER pValue, 
                                   SQLINTEGER cbLen,
                                   SQLINTEGER *pcbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetDescField(FUNC_CALL, 0, phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen, pcbLen);

    rc = RsDesc::RS_SQLGetDescField(phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen, pcbLen, FALSE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetDescField(FUNC_RETURN, rc, phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen, pcbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Helper function for SQLGetDescField and use internally from other functions.
//
SQLRETURN  SQL_API RsDesc::RS_SQLGetDescField(SQLHDESC phdesc,
                                       SQLSMALLINT hRecNumber, 
                                       SQLSMALLINT hFieldIdentifier,
                                       SQLPOINTER pValue, 
                                       SQLINTEGER cbLen,
                                       SQLINTEGER *pcbLen,
                                       int iInternal)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;
    RS_DESC_HEADER &pDescHeader = pDesc->pDescHeader;
    SQLINTEGER *piVal = (SQLINTEGER *)pValue;
    short *phVal = (short *)pValue;
    void **ppVal = (void **)pValue;
    RS_DESC_REC *pDescRec = NULL;
    int iIsHeaderField;
    int iIsReadableField;
    int iDescType;

    if(!VALID_HDESC(phdesc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pDesc->pErrorList = clearErrorList(pDesc->pErrorList);

    if(!pValue)
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"HY000", "Output buffer is NULL", 0, NULL);
        goto error;
    }

    iDescType = pDesc->iType;
    iIsReadableField = (iInternal) ? TRUE : isReadableField(pDesc, hFieldIdentifier);
    iIsHeaderField = isHeaderField(hFieldIdentifier);

    if(!iIsHeaderField)
    {
        // Check for record number.

        // Find if rec exist.
        pDescRec = findDescRec(pDesc, hRecNumber);
        if(pDescRec == NULL)
        {
            if(hRecNumber < 0)
            {
                rc = SQL_ERROR;
                addError(&pDesc->pErrorList,"07009", "Invalid descriptor index", 0, NULL);
                goto error;
            }
            else
            {
                rc = SQL_NO_DATA;
                return rc;
            }
        }
    }

    if(!iIsReadableField)
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"HY091", "Invalid descriptor field identifier", 0, NULL);
        goto error;
    }

    if(iIsHeaderField)
    {
        if(pDesc->pDescHeader.valid == false)
        {
            rc = SQL_ERROR;
            addError(&pDesc->pErrorList,"HY000", "Null pointer found", 0, NULL);
            goto error;
        }
    }

    switch(hFieldIdentifier)
    {
        // Header fields
        case SQL_DESC_ALLOC_TYPE:
        {
            getShortVal(pDescHeader.hAllocType, phVal, pcbLen);
            break;
        }

        case SQL_DESC_ARRAY_SIZE:
        {
            getSQLINTEGERVal(pDescHeader.lArraySize,piVal, pcbLen);
            break;
        }

        case SQL_DESC_ARRAY_STATUS_PTR:
        {
            getPointerVal(pDescHeader.phArrayStatusPtr,ppVal,pcbLen);
            break;
        }

        case SQL_DESC_BIND_OFFSET_PTR:
        {
            getPointerVal(pDescHeader.plBindOffsetPtr,ppVal,pcbLen);
            break;
        }

        case SQL_DESC_BIND_TYPE:
        {
            getSQLINTEGERVal(pDescHeader.lBindType,piVal, pcbLen);
            break;
        }

        case SQL_DESC_COUNT:
        {
            if(pDescHeader.hHighestCount == 0)
                pDescHeader.hHighestCount = findHighestRecCount(pDesc);

            getShortVal(pDescHeader.hHighestCount, phVal, pcbLen);
            break;
        }

        case SQL_DESC_ROWS_PROCESSED_PTR:
        {
            getPointerVal(pDescHeader.plRowsProcessedPtr,ppVal,pcbLen);
            break;
        }

        // Record fields
        case SQL_DESC_AUTO_UNIQUE_VALUE:
        {
            getSQLINTEGERVal(pDescRec->cAutoInc,piVal, pcbLen);
            break;
        }

        case SQL_DESC_BASE_COLUMN_NAME:
        case SQL_DESC_LABEL:
        case SQL_DESC_NAME:
        {
            rc = copyStrDataLargeLen(pDescRec->szName, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_BASE_TABLE_NAME:
        case SQL_DESC_TABLE_NAME:
        {
            rc = copyStrDataLargeLen(pDescRec->szTableName, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_CASE_SENSITIVE:
        {
            getSQLINTEGERVal(pDescRec->cCaseSensitive,piVal, pcbLen);
            break;
        }

        case SQL_DESC_CATALOG_NAME:
        {
            rc = copyStrDataLargeLen(pDescRec->szCatalogName, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_CONCISE_TYPE:
        {
            getShortVal(getConciseType(pDescRec->hConciseType, pDescRec->hType), phVal, pcbLen);
            break;
        }

        case SQL_DESC_TYPE:
        {
            getShortVal(pDescRec->hType, phVal, pcbLen);
            break;
        }

        case SQL_DESC_DATA_PTR:
        {
            getPointerVal(pDescRec->pValue,ppVal,pcbLen);
            break;
        }

        case SQL_DESC_DATETIME_INTERVAL_CODE:
        {
            getShortVal(getDateTimeIntervalCode(pDescRec->hDateTimeIntervalCode,pDescRec->hType),phVal, pcbLen);
            break;
        }

        case SQL_DESC_DATETIME_INTERVAL_PRECISION:
        {
            getSQLINTEGERVal(pDescRec->iDateTimeIntervalPrecision,piVal, pcbLen);
            break;
        }

        case SQL_DESC_DISPLAY_SIZE:
        {
            getSQLINTEGERVal(pDescRec->iDisplaySize,piVal, pcbLen);
            break;
        }

        case SQL_DESC_FIXED_PREC_SCALE:
        {
            getShortVal(pDescRec->cFixedPrecScale, phVal, pcbLen);
            break;
        }

        case SQL_DESC_LENGTH:
        {
            long lLen;

            if(iDescType == RS_IRD || iDescType == RS_IPD || pDescRec->iSize == 0)
                lLen = getSize(pDescRec->hType, pDescRec->iSize);
            else
                lLen = pDescRec->iSize;

            getSQLINTEGERVal(lLen,piVal, pcbLen);

            break;
        }

        case SQL_DESC_LITERAL_PREFIX:
        {
            rc = copyStrDataLargeLen(pDescRec->szLiteralPrefix, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_LITERAL_SUFFIX:
        {
            rc = copyStrDataLargeLen(pDescRec->szLiteralSuffix, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_LOCAL_TYPE_NAME:
        case SQL_DESC_TYPE_NAME:
        {
            rc = copyStrDataLargeLen(pDescRec->szTypeName, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_NULLABLE:
        {
            getShortVal(pDescRec->hNullable, phVal, pcbLen);
            break;
        }

        case SQL_DESC_NUM_PREC_RADIX:
        {
            getSQLINTEGERVal(pDescRec->iNumPrecRadix,piVal, pcbLen);
            break;
        }

        case SQL_DESC_OCTET_LENGTH:
        {
            getSQLINTEGERVal(pDescRec->iOctetLen,piVal, pcbLen);
            break;
        }

        case SQL_DESC_OCTET_LENGTH_PTR:
        {
            getPointerVal(pDescRec->plOctetLen,ppVal,pcbLen);
            break;
        }

        case SQL_DESC_PARAMETER_TYPE:
        {
            getShortVal(pDescRec->hInOutType, phVal, pcbLen);
            break;
        }

        case SQL_DESC_PRECISION:
        {
            getShortVal(pDescRec->iPrecision, phVal, pcbLen);
            break;
        }

        case SQL_DESC_SCALE:
        {
            short hScale;

            if(iDescType == RS_IRD || iDescType == RS_IPD || pDescRec->hScale == 0)
                hScale = getScale(pDescRec->hType, pDescRec->hScale);
            else
                hScale = pDescRec->hScale;

            getShortVal(hScale, phVal, pcbLen);

            break;
        }

        case SQL_DESC_SCHEMA_NAME:
        {
            rc = copyStrDataLargeLen(pDescRec->szSchemaName, SQL_NTS, (char *)pValue, cbLen, pcbLen);
            break;
        }

        case SQL_DESC_SEARCHABLE:
        {
            getShortVal(pDescRec->iSearchable, phVal, pcbLen);
            break;
        }

        case SQL_DESC_UNNAMED:
        {
            getShortVal(pDescRec->iUnNamed, phVal, pcbLen);
            break;
        }

        case SQL_DESC_UNSIGNED:
        {
            getShortVal(pDescRec->cUnsigned, phVal, pcbLen);
            break;
        }

        case SQL_DESC_UPDATABLE:
        {
            getShortVal(pDescRec->iUpdatable, phVal, pcbLen);
            break;
        }

        case SQL_DESC_INDICATOR_PTR:
        {
            getPointerVal(pDescRec->pcbLenInd,ppVal,pcbLen);
            break;
        }

        default:
        {
            rc = SQL_ERROR;
            addError(&pDesc->pErrorList,"HY091", "Invalid descriptor field identifier", 0, NULL);
            goto error;
        }
    } // Switch

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetDescField.
//
SQLRETURN SQL_API SQLGetDescFieldW(SQLHDESC        phdesc,
                                    SQLSMALLINT     hRecNumber,
                                    SQLSMALLINT     hFieldIdentifier,
                                    SQLPOINTER      pwValue,
                                    SQLINTEGER      cbLen,
                                    SQLINTEGER      *pcbLen)
{
    SQLRETURN rc;

    char pValue[MAX_LARGE_TEMP_BUF_LEN] = {0};
    const bool strOption = isStrFieldIdentifier(hFieldIdentifier) != 0;
    // buffer length in SQLWCHARs (including room for NUL if present)
    SQLINTEGER cchLen = 0;

    if (IS_TRACE_LEVEL_API_CALL()) {
        TraceSQLGetDescFieldW(FUNC_CALL, 0, phdesc, hRecNumber,
                              hFieldIdentifier, pwValue, cbLen, pcbLen);
    }
    
    if (!VALID_HDESC(phdesc)) {
        return SQL_INVALID_HANDLE;
    }
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;

    if (strOption) {
        cchLen = (cbLen > 0) ? (cbLen / (SQLINTEGER)sizeofSQLWCHAR()) : 0;
    }

    SQLINTEGER tempLen = 0;
    rc = RsDesc::RS_SQLGetDescField(
        phdesc, hRecNumber, hFieldIdentifier,
        strOption ? (SQLPOINTER)pValue : pwValue,
        strOption ? (SQLINTEGER)(MAX_LARGE_TEMP_BUF_LEN - 1) : cbLen, &tempLen,
        FALSE);

    if (strOption && tempLen >= (MAX_LARGE_TEMP_BUF_LEN - 1)) {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList, "HY000",
                 "Descriptor field exceeds maximum supported size", 0, NULL);
        RS_LOG_ERROR("RSDESC",
                     "Descriptor field exceeds maximum supported size %d",
                     MAX_LARGE_TEMP_BUF_LEN - 1);
        return rc;
    }

    size_t copiedChars = 0; // for tracing only
    if (SQL_SUCCEEDED(rc)) {
        if (strOption) {
            // Convert UTF-8 temp buffer -> SQLWCHAR*
            SQLWCHAR *converted = nullptr;
            size_t totalChars = utf8_to_sqlwchar_alloc(
                (char *)pValue, SQL_NTS, &converted); // excludes NUL

            // Always return required length in BYTES (without NUL)
            if (pcbLen) {
                *pcbLen = (SQLINTEGER)(totalChars * sizeofSQLWCHAR());
            }

            // Write into caller buffer (if provided and size > 0)
            if (pwValue && cchLen > 0 && converted) {
                // Reserve room for NUL
                size_t avail = (cchLen > 0) ? (size_t)cchLen - 1 : 0;
                size_t toCopy = (totalChars < avail) ? totalChars : avail;

                if (toCopy > 0) {
                    memmove(pwValue, converted, toCopy * sizeofSQLWCHAR());
                }
                // NUL-terminate at index toCopy
                setNthSqlwcharNull(pwValue, toCopy);

                // Truncation if source doesn't fit fully (excluding NULL)
                if (totalChars > avail) {
                    rc = SQL_SUCCESS_WITH_INFO;
                    addError(&pDesc->pErrorList, "01004", "string truncated", 0,
                             NULL);
                }
                copiedChars = toCopy;
            }

            // Length-only inquiry (no buffer or zero-size buffer) → SUCCESS
            if (pwValue == NULL || cchLen == 0) {
                rc = SQL_SUCCESS;
                copiedChars = 0;
            }

            converted = (SQLWCHAR *)rs_free(converted);
        } else {
            // Propagate length for non-string fields
            if (pcbLen) {
                *pcbLen = tempLen;
            }
        }
    } else {
        // Propagate length on failure (bytes)
        // Note: pcbLen adjustment on failure for string fields is best effort
        // and can be inaccurate
        if (pcbLen) {
            *pcbLen = tempLen * (strOption ? (SQLINTEGER)sizeofSQLWCHAR() : 1);
        }
    }

    if (IS_TRACE_LEVEL_API_CALL()) {
        TraceSQLGetDescFieldW(
            FUNC_RETURN, rc, phdesc, hRecNumber, hFieldIdentifier, pwValue,
            (SQLINTEGER)(copiedChars * sizeofSQLWCHAR()), pcbLen);
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetDescRec returns the current settings or values of multiple fields of a descriptor record. 
// The fields returned describe the name, data type, and storage of column or parameter data.
//
SQLRETURN  SQL_API SQLGetDescRec(SQLHDESC phdesc,
                                    SQLSMALLINT hRecNumber, 
                                    SQLCHAR *pName,
                                    SQLSMALLINT cbName, 
                                    SQLSMALLINT *pcbName,
                                    SQLSMALLINT *phType, 
                                    SQLSMALLINT *phSubType,
                                    SQLLEN     *plOctetLength, 
                                    SQLSMALLINT *phPrecision,
                                    SQLSMALLINT *phScale, 
                                    SQLSMALLINT *phNullable)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetDescRec(FUNC_CALL, 0, phdesc, hRecNumber, pName, cbName, pcbName, phType, phSubType, plOctetLength, phPrecision, phScale, phNullable);

    rc = RsDesc::RS_SQLGetDescRec(phdesc, hRecNumber, pName, cbName, pcbName, phType, phSubType, plOctetLength, phPrecision, phScale, phNullable);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetDescRec(FUNC_RETURN, rc, phdesc, hRecNumber, pName, cbName, pcbName, phType, phSubType, plOctetLength, phPrecision, phScale, phNullable);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetDescRec and SQLGetDescRecW.
//
SQLRETURN  SQL_API RsDesc::RS_SQLGetDescRec(SQLHDESC phdesc,
                                    SQLSMALLINT hRecNumber, 
                                    SQLCHAR *pName,
                                    SQLSMALLINT cbName, 
                                    SQLSMALLINT *pcbName,
                                    SQLSMALLINT *phType, 
                                    SQLSMALLINT *phSubType,
                                    SQLLEN     *plOctetLength, 
                                    SQLSMALLINT *phPrecision,
                                    SQLSMALLINT *phScale, 
                                    SQLSMALLINT *phNullable)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;
    RS_DESC_REC     *pDescRec;

    if(!VALID_HDESC(phdesc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pDesc->pErrorList = clearErrorList(pDesc->pErrorList); 

    if(hRecNumber < 0 || (hRecNumber == 0 && pDesc->iType == RS_IPD))
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"07009", "Invalid descriptor index", 0, NULL);
        goto error; 
    }

    // Find if rec already exist, then it's re-bind.
    pDescRec = findDescRec(pDesc, hRecNumber);
    if(pDescRec != NULL)
    {
        if(pName)
            copyStrDataSmallLen(pDescRec->szName, MAX_IDEN_LEN, (char *)pName, cbName, pcbName);

        if(phType)
            *phType = pDescRec->hType;

        if(phSubType)
            *phSubType = getDateTimeIntervalCode(pDescRec->hDateTimeIntervalCode,pDescRec->hType);

        if(plOctetLength)
            *plOctetLength = pDescRec->iOctetLen;

        if(phPrecision)
            *phPrecision = pDescRec->iPrecision;

        if(phScale)
            *phScale = pDescRec->hScale;

        if(phNullable)
            *phNullable = pDescRec->hNullable;
    }
    else
        rc = SQL_NO_DATA;

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetDescRec.
//
SQLRETURN SQL_API SQLGetDescRecW(SQLHDESC        phdesc,
                                 SQLSMALLINT     hRecNumber,
                                 SQLWCHAR*         pwName,
                                 SQLSMALLINT     cchName,
                                 SQLSMALLINT     *pcchName,
                                 SQLSMALLINT     *phType,
                                 SQLSMALLINT     *phSubType,
                                 SQLLEN          *plOctetLength,
                                 SQLSMALLINT     *phPrecision,
                                 SQLSMALLINT     *phScale,
                                 SQLSMALLINT     *phNullable)
{
    SQLRETURN rc;
    char szName[MAX_IDEN_LEN + 1] = {0};
    size_t totalCharsNeeded = 0, copiedChars = 0;

    if (!VALID_HDESC(phdesc)) {
        return SQL_INVALID_HANDLE;
    }
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;

    if (IS_TRACE_LEVEL_API_CALL()) {
        TraceSQLGetDescRecW(FUNC_CALL, 0, phdesc, hRecNumber, pwName, cchName,
                            pcchName, phType, phSubType, plOctetLength,
                            phPrecision, phScale, phNullable);
    }

    auto exitLog = make_scope_exit([&]() noexcept {
        if (IS_TRACE_LEVEL_API_CALL()) {
            TraceSQLGetDescRecW(FUNC_RETURN, rc, phdesc, hRecNumber, pwName,
                                copiedChars, pcchName, phType, phSubType,
                                plOctetLength, phPrecision, phScale,
                                phNullable);
        }
    });

    if (cchName < 0 && cchName != SQL_NTS) {
        addError(&pDesc->pErrorList, "HY090", "Invalid string or buffer length",
                 0, NULL);
        rc = SQL_ERROR;
        return rc;
    }

    SQLSMALLINT ansiSize = 0;
    rc = RsDesc::RS_SQLGetDescRec(
        phdesc, hRecNumber, reinterpret_cast<SQLCHAR *>(szName), MAX_IDEN_LEN,
        &ansiSize, phType, phSubType, plOctetLength, phPrecision, phScale,
        phNullable);

    if (SQL_SUCCEEDED(rc)) {
        if (cchName < 0) {
            cchName = 0;
        }

        if (cchName == 0 || !pwName) {
            // Length inquiry only or no room → get required length
            copiedChars = 0;
            totalCharsNeeded = utf8_to_sqlwchar_strlen(szName, SQL_NTS);
        } else {
            copiedChars =
                utf8_to_sqlwchar_str(szName, SQL_NTS, (SQLWCHAR *)pwName,
                                     cchName, &totalCharsNeeded);
        }

        if (pcchName) {
            *pcchName = static_cast<SQLSMALLINT>(totalCharsNeeded);
        }

        if (!pwName) {
            // Length inquiry only: keep rc from RS_SQLGetDescRec
            // i.e., DO NOT force rc = SQL_SUCCESS here.
        } else if (rc == SQL_SUCCESS_WITH_INFO ||
                   copiedChars < totalCharsNeeded) {
            rc = SQL_SUCCESS_WITH_INFO;
        }
        if (ansiSize > MAX_IDEN_LEN && rc == SQL_SUCCESS) {
            // internal bug in RS_SQLGetDescRec: it truncated but didn’t flag it
            std::string err =
                "Internal bug: RS_SQLGetDescRec truncated name but "
                "did not return SQL_SUCCESS_WITH_INFO. ansiSize=" +
                std::to_string(ansiSize) +
                ", MAX_IDEN_LEN=" + std::to_string(MAX_IDEN_LEN);
            RS_LOG_ERROR("RSDESC", "%s", err.c_str());
            rc = SQL_SUCCESS_WITH_INFO;
        }
    }

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSetDescField sets the value of a single field of a descriptor record.
//
SQLRETURN  SQL_API SQLSetDescField(SQLHDESC phdesc,
                                    SQLSMALLINT hRecNumber, 
                                    SQLSMALLINT hFieldIdentifier,
                                    SQLPOINTER pValue, 
                                    SQLINTEGER cbLen)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescField(FUNC_CALL, 0, phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen);

    rc = RsDesc::RS_SQLSetDescField(phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen,FALSE);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescField(FUNC_RETURN, rc, phdesc, hRecNumber, hFieldIdentifier, pValue, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Helper function for  SQLSetDescField and use internally from other functions.
//
SQLRETURN  SQL_API RsDesc::RS_SQLSetDescField(SQLHDESC phdesc,
                                    SQLSMALLINT hRecNumber, 
                                    SQLSMALLINT hFieldIdentifier,
                                    SQLPOINTER pValue, 
                                    SQLINTEGER cbLen,
                                    int iInternal)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;
    RS_DESC_HEADER &pDescHeader = pDesc->pDescHeader;;
    RS_DESC_REC *pDescRec = NULL;
    int iVal = (int)(long)pValue;
    long lVal = 0;
    short hVal = (short)(long)pValue;
    int iIsHeaderField;
    int iIsWritableField;

    lVal = (long)iVal;

    if(!VALID_HDESC(phdesc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pDesc->pErrorList = clearErrorList(pDesc->pErrorList);

    iIsWritableField = (iInternal) ? TRUE : isWritableField(pDesc, hFieldIdentifier);

    if(!iIsWritableField)
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"HY016", "Cannot modify an implementation row descriptor", 0, NULL);
        goto error;
    }

    iIsHeaderField = isHeaderField(hFieldIdentifier);

    if(!iIsHeaderField)
    {
        // Check for record number.
        if(hRecNumber <= 0)
        {
            rc = SQL_ERROR;
            addError(&pDesc->pErrorList,"07009", "Invalid descriptor index", 0, NULL);
            goto error; 
        }

        // Find if rec already exist otherwise add.
        pDescRec = checkAndAddDescRec(pDesc, hRecNumber, TRUE, NULL);
        if(pDescRec == NULL)
        {
            rc = SQL_ERROR;
            if(pDesc->iRecListType ==  RS_DESC_RECS_ARRAY_LIST)
                addError(&pDesc->pErrorList,"HY000", "Cannot add new rec in an array", 0, NULL);
            else
                addError(&pDesc->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }
    }
    else
    {
        if(pDesc->pDescHeader.valid == false)
        {
            rc = SQL_ERROR;
            addError(&pDesc->pErrorList,"HY000", "Null pointer found", 0, NULL);
            goto error;
        }
    }

    switch(hFieldIdentifier)
    {
        // Header fields
        case SQL_DESC_ARRAY_SIZE:
        {
            if(lVal <= 0)
            {
                rc = SQL_ERROR;
                addError(&pDesc->pErrorList,"HY024", "Invalid attribute/option identifier", 0, NULL);
                goto error;
            }

            pDescHeader.lArraySize = lVal;

            break;
        }

        case SQL_DESC_ARRAY_STATUS_PTR:
        {
            pDescHeader.phArrayStatusPtr = (short *)pValue;

            break;
        }

        case SQL_DESC_BIND_OFFSET_PTR:
        {
            pDescHeader.plBindOffsetPtr = (SQLLEN *)pValue;
            break;
        }

        case SQL_DESC_BIND_TYPE:
        {
            if(lVal != SQL_BIND_BY_COLUMN 
                && !(lVal >= 0))
            {
                rc = SQL_ERROR;
                addError(&pDesc->pErrorList,"HY024", "Invalid attribute/option identifier", 0, NULL);
                goto error;
            }

            pDescHeader.lBindType = lVal;

            break;
        }

        case SQL_DESC_COUNT:
        {
            RS_DESC_REC *pTempDescRec;
            RS_DESC_REC *pTempNextDescRec;
            int zeroAllowed = ((pDesc->iType == RS_APD || pDesc->iType == RS_ARD)
                                && (pDesc->iRecListType == RS_DESC_RECS_LINKED_LIST));

            if(hVal < 0 || (hVal == 0 && !zeroAllowed))
            {
                rc = SQL_ERROR;
                addError(&pDesc->pErrorList,"HY024", "Invalid attribute/option identifier", 0, NULL);
                goto error;
            }

            pDescHeader.hHighestCount = hVal;

            if(hVal != 0)
            {
                // Check if we need to create the record
                pDescRec = checkAndAddDescRec(pDesc, hVal, TRUE, NULL);
            }

            // Remove records above the given number
            // For ARD and APD if count is 0 then we remove all records.
            // Loop through rec list
            for(pTempDescRec  = pDesc->pDescRecHead;pTempDescRec != NULL;pTempDescRec = pTempNextDescRec)
            {
                pTempNextDescRec = pTempDescRec->pNext;
                if(pTempDescRec->hRecNumber > hVal)
                    releaseDescriptorRec(pDesc, pTempDescRec);
            }

            break;
        }

        case SQL_DESC_ROWS_PROCESSED_PTR:
        {
            pDescHeader.plRowsProcessedPtr = (long *)pValue;

            break;
        }

        // Rec fields

        case SQL_DESC_DATA_PTR:
        {
            pDescRec->pValue = pValue;
            break;
        }

        case SQL_DESC_INDICATOR_PTR:
        {
            pDescRec->pcbLenInd = (SQLLEN *)pValue;
            break;
        }

        case SQL_DESC_LENGTH:
        {
            pDescRec->iSize = iVal;
            break;
        }

        case SQL_DESC_NAME:
        {
            copyStrDataSmallLen((char *)pValue, cbLen, pDescRec->szName, MAX_IDEN_LEN, NULL);
            break;
        }

        case SQL_DESC_NUM_PREC_RADIX:
        {
            pDescRec->iNumPrecRadix = iVal;
            break;
        }

        case SQL_DESC_OCTET_LENGTH:
        {
            pDescRec->iOctetLen = iVal;

            if(pDesc->iType == RS_APD
                 || pDesc->iType == RS_ARD)
            {
                if(pDescRec->cbLen == 0)
                {
                    pDescRec->cbLen  = iVal;
                }
            }

            break;
        }

        case SQL_DESC_OCTET_LENGTH_PTR:
        {
            pDescRec->plOctetLen = (SQLINTEGER *)pValue;
            break;
        }

        case SQL_DESC_PARAMETER_TYPE:
        {
            pDescRec->hInOutType = hVal;
            break;
        }

        case SQL_DESC_PRECISION:
        {
            pDescRec->iPrecision = hVal;
            break;
        }

        case SQL_DESC_SCALE:
        {
            pDescRec->hScale = hVal;
            break;
        }

        case SQL_DESC_TYPE:
        {
            pDescRec->hType = hVal;
            break;
        }

        case SQL_DESC_CONCISE_TYPE:
        {
            pDescRec->hConciseType = hVal;

            if(pDesc->iType == RS_APD
                 || pDesc->iType == RS_ARD
                 || pDesc->iType == RS_IPD)
            {
                pDescRec->hType = getCTypeFromConciseType(hVal, pDescRec->hDateTimeIntervalCode, pDescRec->hType);
            }

            break;
        }

        case SQL_DESC_UNNAMED:
        {
            pDescRec->iUnNamed = hVal;
            break;
        }

        case SQL_DESC_AUTO_UNIQUE_VALUE:
        case SQL_DESC_BASE_COLUMN_NAME:
        case SQL_DESC_BASE_TABLE_NAME:
        case SQL_DESC_CASE_SENSITIVE:
        case SQL_DESC_CATALOG_NAME:
        case SQL_DESC_DISPLAY_SIZE:
        case SQL_DESC_FIXED_PREC_SCALE:
        case SQL_DESC_LABEL:
        case SQL_DESC_LITERAL_PREFIX:
        case SQL_DESC_LITERAL_SUFFIX:
        case SQL_DESC_LOCAL_TYPE_NAME:
        case SQL_DESC_NULLABLE:
        case SQL_DESC_SCHEMA_NAME:
        case SQL_DESC_SEARCHABLE:
        case SQL_DESC_TABLE_NAME:
        case SQL_DESC_TYPE_NAME:
        case SQL_DESC_UNSIGNED:
        case SQL_DESC_UPDATABLE:
        {
            // Do nothing. Unused.
            break;
        }

        case SQL_DESC_DATETIME_INTERVAL_CODE:
        {
            pDescRec->hDateTimeIntervalCode = hVal;
            break;
        }

        case SQL_DESC_DATETIME_INTERVAL_PRECISION:
        {
            pDescRec->iDateTimeIntervalPrecision = iVal;
            break;
        } 

        default:
        {
            rc = SQL_ERROR;
            addError(&pDesc->pErrorList,"HY091", "Invalid descriptor field identifier", 0, NULL);
            goto error;
        }
    } // Switch

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLSetDescField.
//
SQLRETURN  SQL_API SQLSetDescFieldW(SQLHDESC        phdesc,
                                    SQLSMALLINT     hRecNumber,
                                    SQLSMALLINT     hFieldIdentifier,
                                    SQLPOINTER      pwValue,
                                    SQLINTEGER      cbLen)
{
    SQLRETURN rc;
    char szName[MAX_IDEN_LEN + 1];
    int isWritableString = FALSE;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescFieldW(FUNC_CALL, 0, phdesc, hRecNumber, hFieldIdentifier, pwValue, cbLen);

    if(hFieldIdentifier == SQL_DESC_NAME)
    {
        isWritableString = TRUE;

        if (pwValue) {
            sqlwchar_to_utf8_char((SQLWCHAR *)pwValue,
                                  (cbLen > 0) ? cbLen / sizeofSQLWCHAR()
                                              : cbLen,
                                  szName, MAX_IDEN_LEN);
        }
    }

    rc = RsDesc::RS_SQLSetDescField(phdesc, hRecNumber, hFieldIdentifier, (isWritableString && pwValue) ? szName : pwValue,
                            (isWritableString && pwValue) ? MAX_IDEN_LEN : cbLen, FALSE);


    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescFieldW(FUNC_RETURN, rc, phdesc, hRecNumber, hFieldIdentifier, pwValue, cbLen);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// The SQLSetDescRec function sets multiple descriptor fields that affect the data type and buffer bound to a column or parameter data. 
//
SQLRETURN  SQL_API SQLSetDescRec(SQLHDESC phdesc,
                                    SQLSMALLINT hRecNumber, 
                                    SQLSMALLINT hType,
                                    SQLSMALLINT hSubType, 
                                    SQLLEN        iOctetLength,
                                    SQLSMALLINT hPrecision, 
                                    SQLSMALLINT hScale,
                                    SQLPOINTER    pData, 
                                    SQLLEN *    plStrLen,
                                    SQLLEN *    plIndicator)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_DESC_INFO *pDesc = (RS_DESC_INFO *)phdesc;
    RS_DESC_REC     *pDescRec;
    int iIsWritableField;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescRec(FUNC_CALL, 0, phdesc, hRecNumber, hType, hSubType, iOctetLength, hPrecision, hScale, pData, plStrLen, plIndicator);

    if(!VALID_HDESC(phdesc))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pDesc->pErrorList = clearErrorList(pDesc->pErrorList); 

    if(hRecNumber <= 0)
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"07009", "Invalid descriptor index", 0, NULL);
        goto error; 
    }

    iIsWritableField = isWritableField(pDesc, SQL_DESC_TYPE);
    if(iIsWritableField)
    {
        iIsWritableField = isWritableField(pDesc, SQL_DESC_DATETIME_INTERVAL_CODE);
        if(iIsWritableField)
        {
            iIsWritableField = isWritableField(pDesc, SQL_DESC_OCTET_LENGTH);
            if(iIsWritableField)
            {
                iIsWritableField = isWritableField(pDesc, SQL_DESC_PRECISION);
                if(iIsWritableField)
                {
                    iIsWritableField = isWritableField(pDesc, SQL_DESC_SCALE);
                    if(iIsWritableField)
                    {
                        iIsWritableField = isWritableField(pDesc, SQL_DESC_DATA_PTR);
                        if(iIsWritableField)
                        {
                            iIsWritableField = isWritableField(pDesc, SQL_DESC_OCTET_LENGTH_PTR);
                            if(iIsWritableField)
                            {
                                iIsWritableField = isWritableField(pDesc, SQL_DESC_INDICATOR_PTR);
                            }
                        }
                    }
                }
            }
        }
    }

    if(!iIsWritableField)
    {
        rc = SQL_ERROR;
        addError(&pDesc->pErrorList,"HY000", "Field is not writable", 0, NULL);
        goto error;
    }

    if(pData == NULL && plIndicator == NULL && pDesc->iType == RS_ARD)
    {
        // Unbind it
        releaseDescriptorRecByNum(pDesc, hRecNumber);
    }
    else
    {
        // Find if rec already exist, then it's re-bind.
        pDescRec = checkAndAddDescRec(pDesc, hRecNumber, TRUE, NULL);
        if(pDescRec == NULL)
        {
            rc = SQL_ERROR;
            if(pDesc->iRecListType ==  RS_DESC_RECS_ARRAY_LIST)
                addError(&pDesc->pErrorList,"HY000", "Cannot add new rec in an array", 0, NULL);
            else
                addError(&pDesc->pErrorList,"HY001", "Memory allocation error", 0, NULL);
            goto error;
        }

        // Set values
        if(pDescRec)
        {
            pDescRec->hType    = hType;
            pDescRec->hDateTimeIntervalCode = hSubType;
            pDescRec->iOctetLen = (int) iOctetLength;
            pDescRec->iPrecision = hPrecision;
            pDescRec->hScale = hScale;
            pDescRec->pValue = pData;
            pDescRec->plOctetLen = (SQLINTEGER *)plStrLen;
            pDescRec->pcbLenInd = plIndicator;

            if(pDesc->iType == RS_APD
                 || pDesc->iType == RS_ARD)
            {
                if(pDescRec->cbLen == 0)
                {
                    pDescRec->cbLen  = iOctetLength;
                }
            }
        }
    }

error:

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSetDescRec(FUNC_RETURN, rc, phdesc, hRecNumber, hType, hSubType, iOctetLength, hPrecision, hScale, pData, plStrLen, plIndicator);

    return rc;
}
