#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <getopt.h>
#else
#include <WinSock2.h>
#include <stdint.h>
#include <io.h>
#include "getopt.h"
#endif

#include <signal.h>
#include <errno.h>
#include <fcntl.h>


#ifndef _WIN32
#include <logger.h>
#else
#include "logger.h"
#endif



#define LF  (uint8_t) 10
#define CR  (uint8_t) 13

#ifdef _MSC_VER
int 
kill(int32_t pid, int32_t n)
{
	HANDLE hand=OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hand!=NULL)
	{
		 if (TerminateProcess(hand, n))
		 {
			 CloseHandle(hand);
			 return 1;
		 }
	}
	return -1;
}

static int
read_file(int fd, uint8_t *buf, size_t size, off_t offset) {
	int n = read(fd, buf, size);
	if (n == -1) {
		return -1;
	}
	return n;
}

#define ssize_t intptr_t

#else
static ssize_t
read_file(int fd, uint8_t *buf, size_t size, off_t offset) {
	ssize_t n = pread(fd, buf, size, offset);
	if (n == -1) {
		return -1;
	}
	return n;
}

#endif

static int32_t
get_pid(const char *pidfile) {
    u_char    buf[10];
    ssize_t   n;

    int fd = open(pidfile, O_RDONLY);

    if (fd == -1) {
        return -1;
    }

    n = read_file(fd, buf, 10, 0);

    close(fd);

    if (n == -1) {
        return -1;
    }

    while (n-- && (buf[n] == CR || buf[n] == LF)) { /* void */ }

    return atoi((const char*)buf);
}

int
signal_process(char *signal, const char *pidfile) {
    int32_t  pid;

    pid = get_pid(pidfile);
    if (pid == -1) {
		logger_log(LOG_ERR, "open \"%s\" failed (%d: %s)", pidfile, errno, strerror(errno));
        return 1;
    }

    if (strcmp(signal, "stop") == 0) {
        if (kill(pid, SIGTERM) != -1) {
            return 0;
        } else {
            logger_log(LOG_ERR, "stop socksd failed (%d: %s)", pid, strerror(errno));
        }
    }
    if (strcmp(signal, "quit") == 0) {
#ifndef _MSC_VER
		if (kill(pid, SIGQUIT) != -1) {
#else
		if (kill(pid, SIGABRT_COMPAT) != -1) {
#endif
            return 0;
        } else {
            logger_log(LOG_ERR, "quit socksd failed (%d: %s)", errno, strerror(errno));
        }
    }

    return 1;
}
