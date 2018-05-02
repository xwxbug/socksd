#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#ifndef _WIN32
#include <syslog.h>
#else
#include "syslog.h"
#endif

#define LOG_MESSAGE_SIZE 256

static int _syslog = 0;

static char *levels[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG"
};

#ifdef _MSC_VER
static char *colors[] = {
	"", "", "", "", "", "", "", ""
};
#else
static char *colors[] = {
	"\e[01;31m", "\e[01;31m", "\e[01;31m", "\e[01;31m", "\e[01;33m", "\e[01;33m", "\e[01;32m", "\e[01;36m"
};
#endif

#ifdef _WIN32
int 
syslog(uint32_t level, char const* const _Format, ...)
{
	return 0;
}
#endif

void
logger_log(uint32_t level, const char *msg, ...) {
    char timestr[20];
    time_t curtime = time(NULL);
    struct tm *loctime = localtime(&curtime);

	char tmp[LOG_MESSAGE_SIZE];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
	va_end(ap);
	
    if (_syslog) {
        syslog(level, "[%s] %s", levels[level], tmp);
    } else {
        strftime(timestr, 20, "%Y/%m/%d %H:%M:%S", loctime);
#ifdef _MSC_VER
        fprintf(stderr, "%s%s [%s]: %s\n", colors[level], timestr, levels[level], tmp);
#else
        fprintf(stderr, "%s%s [%s]\e[0m: %s\n", colors[level], timestr, levels[level], tmp);
#endif
    }
}

void
logger_stderr(const char *msg, ...) {
    char timestr[20];
    time_t curtime = time(NULL);
    struct tm *loctime = localtime(&curtime);

	char tmp[LOG_MESSAGE_SIZE];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
	va_end(ap);

    strftime(timestr, 20, "%Y/%m/%d %H:%M:%S", loctime);
    fprintf(stderr, "\e[01;31m%s [%s]\e[0m: %s\n", timestr, levels[LOG_ERR], tmp);
}

int
logger_init(int syslog) {
    _syslog = syslog;
    return 0;
}
