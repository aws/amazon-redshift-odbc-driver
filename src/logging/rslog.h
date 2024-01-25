#ifndef RS_LOGGER_H
#define RS_LOGGER_H
#pragma once
#include <stdio.h> // FILE*
#include <stdarg.h> // __VA_ARGS__

#include <string.h>

/*
 * Global log state.
 * Important for producing/consuming settings across modules
 */
struct RS_LOG_VARS {
    int iTraceLevel;                // Trace level
    char szTraceFile[300];          // Trace file name
    int isInitialized;
    //Deprecated
    FILE *fpTrace;                  // Trace file handle
};

// Public C-Style interface
#ifdef __cplusplus
extern "C" {
#endif
void initializeLogging();
void shutdownLogging();
int getRsLoglevel();
void setRsLogLevel(int level);
struct RS_LOG_VARS *getGlobalLogVars();


void RS_LOG_FATAL_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_LOG_ERROR_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_LOG_WARN_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_LOG_INFO_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_LOG_DEBUG_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_LOG_TRACE_(const char* file, const int line, const char* func, const char *tag, const char *fmt, ...);
void RS_STREAM_LOG_TRACE_(const char* file, const int line, const char* func, const char *tag, const char *s, long long len);

#define RS_LOG_FATAL(...) RS_LOG_FATAL_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__) 
#define RS_LOG_ERROR(...) RS_LOG_ERROR_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__) 
#define RS_LOG_WARN(...) RS_LOG_WARN_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__) 
#define RS_LOG_INFO(...) RS_LOG_INFO_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__) 
#define RS_LOG_DEBUG(...) RS_LOG_DEBUG_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__) 
#define RS_LOG_TRACE(...) RS_LOG_TRACE_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
/*
Traces the streams with encodings that have problem with size aware print formats like %.*s.
This function is only provided for backward compatibility. It is expensive and therefore 
, for now, is hard-coded for trace level only.
*/
#define RS_STREAM_LOG_TRACE(...) RS_STREAM_LOG_TRACE_(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#ifdef __cplusplus
}
#endif

// Recognized log levels
#define LOG_LEVEL_OFF 0
#define LOG_LEVEL_FATAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6


// Legacy macros and mapping
#define DEFAULT_TRACE_LEVEL LOG_LEVEL_OFF
#define TRACE_FILE_NAME "redshift_odbc.log"
#define IS_TRACE_ON() (getRsLoglevel() > LOG_LEVEL_OFF)
#define IS_TRACE_LEVEL_ERROR() (getRsLoglevel() >= LOG_LEVEL_ERROR)
#define IS_TRACE_LEVEL_API_CALL() (getRsLoglevel() >= LOG_LEVEL_INFO)
#define IS_TRACE_LEVEL_INFO() (getRsLoglevel() >= LOG_LEVEL_INFO)
#define IS_TRACE_LEVEL_MSG_PROTOCOL() (getRsLoglevel() >= LOG_LEVEL_DEBUG)
#define IS_TRACE_LEVEL_DEBUG() (getRsLoglevel() >= LOG_LEVEL_DEBUG)
#define IS_TRACE_LEVEL_DEBUG_APPEND() (getRsLoglevel() >= LOG_LEVEL_TRACE)

// previous log level macros (for reference only)
/* legacy log levels:
    #define TRACE_OFF               0
    #define TRACE_ERROR             1
    #define TRACE_API_CALL          2
    #define TRACE_INFO              3
    #define TRACE_MSG_PROTOCOL      4
    #define TRACE_DEBUG             5
    #define TRACE_DEBUG_APPEND_MODE 6
*/

#endif //RS_LOGGER_H