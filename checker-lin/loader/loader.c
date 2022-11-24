// SPDX-License-Identifier: EUPL-1.2
/* Copyright Mitran Andrei-Gabriel 2022 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "exec_parser.h"

/* Defensive programming macro */
#define DIE(assertion, call_description)		\
	do {										\
		if (assertion) {						\
			fprintf(stderr, "(%s, %d): ",		\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(errno);						\
		}										\
	} while (0)

/* Getting a single page's size according to the system */
#define PAGE_SIZE getpagesize()

#define uint unsigned int

static so_exec_t *exec;

/* Declared fd static so that segv_handler can use it */
static int fd;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	/* The handler is designed and tested for SIGSEGV */
	DIE(signum != SIGSEGV, "not SIGSEGV");

	/* Signal's information not provided */
	DIE(!info, "no siginfo");

	/* Where the page fault occured */
	uintptr_t fault_address = (uintptr_t)info->si_addr;

	for (int i = 0; i < exec->segments_no; ++i) {
		so_seg_t *segment = exec->segments + i;

		uint mem_size = segment->mem_size;

		/* Allocating memory for a vector in which to set whether or not a page
		 * has been mapped already (in this segment)
		 * Avoided using malloc or calloc
		 */
		if (!segment->data) {
			segment->data = mmap(NULL, mem_size / PAGE_SIZE + 1, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

			DIE(!segment->data, "mmap failed()");
		}

		uintptr_t segment_start = segment->vaddr;

		/* The number of pages we need to go over to reach the page with the fault */
		uint page_cnt = (fault_address - segment_start) / PAGE_SIZE;

		/* Checking if the page has already been mapped */
		if (((char *)segment->data)[page_cnt] != 0) {
			/* The original handler is called, permissions are invalid */
			signal(SIGSEGV, SIG_DFL);

			return;
		}

		/* If the fault lies outside this segment, we can move on to the next segment */
		if (fault_address >= segment_start + mem_size || fault_address < segment_start)
			continue;

		/* Mapping the page */
		void *mmap_ret = mmap((void *)(segment_start + page_cnt * PAGE_SIZE), PAGE_SIZE,
			PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

		DIE(mmap_ret == MAP_FAILED, "mmap() failed");

		uint file_size = segment->file_size;
		int read_size = file_size - page_cnt * PAGE_SIZE;

		/* Checking if there is something to be read */
		if (read_size >= 0) {
			/* Getting to the page from which to read (+ defensive programming) */
			DIE(lseek(fd, segment->offset + page_cnt * PAGE_SIZE, SEEK_SET) < 0,
				"lseek() failed");

			/* Storing what is needed at the newly mapped memory (+ defensive programming) */
			/* May read a full page or not depending on whether the memory is alligned or not */
			DIE(read(fd, mmap_ret, read_size < PAGE_SIZE ? read_size : PAGE_SIZE) == -1,
				"read() failed");
		}

		/* Marking the page as being mapped */
		((char *)segment->data)[page_cnt] = 1;

		/* Setting the permissions (+ defensive programming) */
		DIE(mprotect(mmap_ret, PAGE_SIZE, segment->perm) == -1, "mprotect failed()");
		return;
	}

	/* The fault was not found in the segments, so the default handler is called */
	signal(SIGSEGV, SIG_DFL);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

/* Tried to not modify the function too much */
int so_execute(char *path, char *argv[])
{
	/* Opening the file, to get the file descriptor (+ defensive programming) */
	fd = open(path, O_RDONLY);
	DIE(fd < 0, "open failed()");

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	/* Closing the file */
	close(fd);

	return -1;
}
