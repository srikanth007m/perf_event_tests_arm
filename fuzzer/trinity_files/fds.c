#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#if VMW
#include "epoll.h"
#include "eventfd.h"
#include "files.h"
#endif
#include "log.h"
#if VMW
#include "net.h"
#include "params.h"
#include "perf.h"
#include "pids.h"
#include "pipes.h"
#include "random.h"
#include "sanitise.h"
#include "shm.h"
#include "trinity.h"

static int get_new_random_fd(void)
{
	unsigned int i;
	int fd = 0;

retry:
	i = rand() % 6;

	if (do_specific_proto == TRUE)
		i = 1;

	if (no_files == TRUE)
		i = 1;

	/* Ugly special case.
	 * Sometimes, we can get here without any fd's setup.
	 * If this happens, we divide by zero if we pick case 0 because
	 * nr_file_fds is zero
	 *
	 * When this circumstance occurs, we just force it to use another network socket.
	 *
	 * FIXME: A better solution would be to like, actually open an fd. duh.
	 */
	if (nr_file_fds == 0)
		i = 1;

	switch (i) {
	case 0:
		fd = rand_file_fd();
		break;

	case 1:
		/* When using victim files, sockets can be 0.
		 * Use files as a fallback, or pipes if no files are open.
		 */
		if (nr_sockets == 0) {
			if (nr_file_fds > 0)
				fd = rand_file_fd();
			else
				fd = rand_pipe_fd();
			return fd;
		}
		fd = shm->sockets[rand() % nr_sockets].fd;
		break;

	case 2:
		fd = rand_pipe_fd();
		break;

	case 3:
		if (shm->perf_fds[0] == 0)	/* perf unavailable/disabled. */
			goto retry;

		fd = rand_perf_fd();
		break;

	case 4:
		fd = rand_epoll_fd();
		break;

	case 5:
		fd = rand_eventfd_fd();
		break;

	}

	return fd;
}
#endif


int get_random_fd(void)
{

	return rand()%30000;
#if VMW

	/* 25% chance of returning something new. */
	if ((rand() % 4) == 0)
		return get_new_random_fd();

	/* the rest of the time, return the same fd as last time. */
regen:
	if (shm->fd_lifetime == 0) {
		shm->current_fd = get_new_random_fd();
		shm->fd_lifetime = rand_range(5, max_children);
	} else
		shm->fd_lifetime--;

	if (shm->current_fd == 0) {
		shm->fd_lifetime = 0;
		goto regen;
	}

	return shm->current_fd;
#endif
}

#if VMW
unsigned int setup_fds(void)
{
	int ret = TRUE;

	/* If we have victim files, don't worry about sockets. */
	if (victim_path == NULL) {
		ret = open_sockets();
		if (ret == FALSE)
			return FALSE;
	}

	open_pipes();

	open_perf_fds();

	open_epoll_fds();

	open_eventfd_fds();

	if (no_files == FALSE)
		ret = open_files();

	return ret;
}

void regenerate_fds(void)
{
	if (no_files == TRUE)
		return;

	close_files();
	open_files();
}
#endif
