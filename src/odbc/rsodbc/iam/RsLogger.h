
#ifndef _RS_LOGGER_H_
#define _RS_LOGGER_H_

#define RS_LOG(logger) if(logger && logger->traceEnable) logger->log

class RsLogger {

public :
    bool traceEnable;
    static void log(char *fmt, ...);
};



#endif
