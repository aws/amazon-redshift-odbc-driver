/*-------------------------------------------------------------------------
*
* Copyright(c) 2026, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
*-------------------------------------------------------------------------
*/

/*
 * SSL helpers shared by the driver and its tests: CA file checks, which modes load the
 * built-in root.crt, and where libpq looks for the user's default CA file.
 */
#ifndef RS_SSL_COMMON_H
#define RS_SSL_COMMON_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#else
#include <strings.h>
#include <unistd.h>
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#endif

/* What is wrong with a CA file path. Opening is not enough: a directory opens on glibc
 * and stat() succeeds on a file the process cannot read. */
enum class TrustedCaFileState
{
    Ok,
    NotFound,    /* stat() failed; errno says why */
    NotRegular,  /* a directory, device or socket */
    NotReadable  /* there, but not readable */
};

/** What parsing a CA file with OpenSSL found. InternalError means OpenSSL ran out of memory. */
enum class CaLoadStatus { Loaded, Invalid, InternalError };

/**
 * Tells whether a path is a file the process can read, before OpenSSL parses it.
 * @param pSavedErrno Receives errno from the failing call, else 0; may be NULL.
 * @return A TrustedCaFileState, the first thing found wrong.
 */
static inline TrustedCaFileState
rs_ssl_ca_file_state(const char *path, int *pSavedErrno)
{
#ifdef WIN32
    struct _stat info;
#else
    struct stat info;
#endif

    if (pSavedErrno != NULL) {
        *pSavedErrno = 0;
    }

    if (path == NULL || path[0] == '\0') {
        if (pSavedErrno != NULL) {
            *pSavedErrno = ENOENT;
        }
        return TrustedCaFileState::NotFound;
    }

    /* Each check runs only after the one before it passed: info is unset until stat
     * succeeds, and access() would overwrite the errno stat left. */
#ifdef WIN32
    const bool statFailed = _stat(path, &info) != 0;
#else
    const bool statFailed = stat(path, &info) != 0;
#endif
    if (statFailed) {
        if (pSavedErrno != NULL) {
            *pSavedErrno = errno;
        }
        return TrustedCaFileState::NotFound;
    }

#ifdef WIN32
    const bool notRegular = (info.st_mode & _S_IFMT) != _S_IFREG;
#else
    const bool notRegular = !S_ISREG(info.st_mode);
#endif
    if (notRegular) {
        return TrustedCaFileState::NotRegular;
    }

#ifdef WIN32
    /* _access takes no R_OK; 4 is its read-permission bit. */
    const bool notReadable = _access(path, 4) != 0;
#else
    const bool notReadable = access(path, R_OK) != 0;
#endif
    if (notReadable) {
        if (pSavedErrno != NULL) {
            *pSavedErrno = errno;
        }
        return TrustedCaFileState::NotReadable;
    }

    return TrustedCaFileState::Ok;
}

/* Bytes searched for the marker; a PEM bundle starts with its first certificate. */
#define RS_SSL_CA_FILE_PROBE_BYTES 4096

/**
 * Tells whether a file looks like the PEM bundle libpq loads. A screen, not a parse
 * (the built-in root.crt is large), so a malformed block can still pass.
 * @return Nonzero when a "-----BEGIN " marker appears near the start.
 */
static inline int
rs_ssl_ca_file_has_certificate_header(const char *path)
{
    char buf[RS_SSL_CA_FILE_PROBE_BYTES + 1];
    FILE *caFile;
    size_t got;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    caFile = fopen(path, "rb");
    if (caFile == NULL) {
        return 0;
    }

    got = fread(buf, 1, RS_SSL_CA_FILE_PROBE_BYTES, caFile);
    fclose(caFile);

    buf[got] = '\0';
    return strstr(buf, "-----BEGIN ") != NULL ? 1 : 0;
}

/**
 * Tells whether an SSL mode always validates the server certificate: verify-ca or verify-full.
 * These modes load the built-in root.crt even without a caller or ambient CA. The allow,
 * prefer and require modes validate when CA resolution finds a usable caller or ambient CA.
 */
static inline int
rs_ssl_mode_verifies_cert(const char *sslMode)
{
    if (sslMode == NULL) {
        return 0;
    }
    return (_stricmp(sslMode, "verify-ca") == 0 ||
            _stricmp(sslMode, "verify-full") == 0) ? 1 : 0;
}

/* What libpq appends to the home directory to find the user's default root.crt (its
 * ROOT_CERT_FILE). Windows already reports a Redshift-specific home directory. */
#ifndef WIN32
#define RS_SSL_USER_DEFAULT_CERT_SUFFIX ".redshift/root.crt"
#else
#define RS_SSL_USER_DEFAULT_CERT_SUFFIX "root.crt"
#endif

/**
 * Builds the path of the user's default root.crt, the CA file libpq loads from the home
 * directory. Composed here so a verify mode can add the built-in root.crt beside it.
 * @param buf Receives the path; untouched unless the return is nonzero.
 * @return Nonzero when buf holds the whole path; zero if no home directory or too long.
 */
static inline int
rs_ssl_compose_user_default_cert_path(const char *homeDir, char *buf,
                                      size_t bufSize)
{
    const size_t suffixLen = sizeof(RS_SSL_USER_DEFAULT_CERT_SUFFIX) - 1;
    size_t homeLen;

    if (homeDir == NULL || homeDir[0] == '\0' || buf == NULL) {
        return 0;
    }

    homeLen = strlen(homeDir);
    /* homeDir + '/' + suffix + '\0' */
    if (homeLen + 1 + suffixLen + 1 > bufSize) {
        return 0;
    }

    memcpy(buf, homeDir, homeLen);
    buf[homeLen] = '/';
    memcpy(buf + homeLen + 1, RS_SSL_USER_DEFAULT_CERT_SUFFIX, suffixLen + 1);
    return 1;
}

/* libpq's MAXPGPATH, the buffer libpq composes this path in. */
#define RS_SSL_USER_DEFAULT_CERT_PATH_MAX 1024

/**
 * Tells whether the user's default root.crt is there and readable. Takes the directory
 * because tests cannot redirect pqGetHomeDirectory(). A look-up, not a request, so an
 * absent file must not fail the connection.
 */
static inline int
rs_ssl_user_default_cert_present(const char *homeDir)
{
    char path[RS_SSL_USER_DEFAULT_CERT_PATH_MAX];

    if (!rs_ssl_compose_user_default_cert_path(homeDir, path, sizeof(path))) {
        return 0;
    }

    return rs_ssl_ca_file_state(path, NULL) == TrustedCaFileState::Ok ? 1 : 0;
}

#endif /* RS_SSL_COMMON_H */
