#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdint.h>
#ifndef _WIN32
#include <syslog.h>
#else
#include "syslog.h"
#endif

int logger_init(int syslog);
void logger_stderr(const char *msg, ...);
void logger_log(uint32_t level, const char *msg, ...);

#endif // for #ifndef _LOGGER_H
