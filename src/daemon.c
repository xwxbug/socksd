#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/file.h>
#else
#include <io.h>
#include <sys/locking.h>
#include <sys/types.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "logger.h"


#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)


static int
lockfile(int fd) {
#ifdef _WIN32
	return _locking(fd, _LK_NBLCK, 1);
#else
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd, F_SETLK, &fl));
#endif
}

int
already_running(const char *pidfile) {
	int	  fd;
    char  buf[16];

#ifdef _MSC_VER
    fd = open(pidfile, O_RDWR | O_CREAT, _S_IREAD|_S_IWRITE);
#else
    fd = open(pidfile, O_RDWR | O_CREAT, LOCKMODE);
#endif

	if (fd < 0) {
		logger_log(LOG_ERR, "open \"%s\" failed (%d: %s)", pidfile, errno, strerror(errno));
		exit(1);
	}
	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return(1);
		}
		logger_log(LOG_ERR, "can't lock %s: %s\n", pidfile, strerror(errno));
		exit(1);
	}

    /*
     * create pid file
     */
#ifndef _MSC_VER
    if (ftruncate(fd, 0)) {
		fprintf(stderr, "can't truncate %s: %s", pidfile, strerror(errno));
		exit(1);
    }
#endif
    sprintf(buf, "%ld\n", (long)getpid());
    if (write(fd, buf, strlen(buf)+1) == -1) {
		fprintf(stderr, "can't write %s: %s", pidfile, strerror(errno));
		exit(1);
    }

	return(0);
}


void
create_pidfile(const char *pidfile) {
    FILE  *fp = fopen(pidfile, "w");
    if (fp) {
        fprintf(fp, "%ld\n", (long)getpid());
        fclose(fp);
    }
}


void
delete_pidfile(const char *pidfile) {
    unlink(pidfile);
}

#ifdef _MSC_VER
int
fork() {

	return -1;
}

int
daemonize(void) {
	int    fd;
	unsigned long  pid;

	switch (pid = fork()) {
	case -1:
		fprintf(stderr, "fork() failed.\n");
		return -1;

	case 0:
		break;

	default:
		exit(0);
	}
#ifndef _MSC_VER
	setsid();
#endif
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, stdin);
		dup2(fd, stdout);
		/* dup2(fd, STDERR_FILENO); */
	}

	return 0;
}

#else
int
daemonize(void) {
	int    fd;
	pid_t  pid;

	switch (pid = fork()) {
	case -1:
		fprintf(stderr, "fork() failed.\n");
		return -1;

	case 0:
		break;

	default:
		exit(0);
	}

	setsid();

	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		/* dup2(fd, STDERR_FILENO); */
	}

	return 0;
}

#endif

