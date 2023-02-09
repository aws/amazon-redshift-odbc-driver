#include "RsLogger.h"
#include <rstrace.h>
#include <stdarg.h>

// Deprecated
// void traceDebugWithArgList(char *fmt, va_list args);

void RsLogger::log(char *fmt,...)
{
  va_list args;         \
  va_start(args, fmt);  \

  traceDebugWithArgList(fmt,args);

  va_end(args);
}
