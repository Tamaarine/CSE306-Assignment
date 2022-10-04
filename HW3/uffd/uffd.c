/* userfaultfd_demo.c

   Licensed under the GNU General Public License version 2 or later.
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);	\
	} while (0)

static int page_size;

static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;     /* Number of faults so far handled */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	/* [H1]
	 * Explain following in here.
	 * The line of code here is allocating another 1 page for the thread
	 * using mmap. It uses the same configuration from main as described
	 * anonymous mapping, is private, and is allowed to read and write to the
	 * memory mapped.
	 * 
	 * If it failed then exit the thread.
	 */
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	/* [H2]
	 * Explain following in here.
	 * This is an infinite for loop
	 */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */

		struct pollfd pollfd;
		int nready;

		/* [H3]
		 * Explain following in here.
		 * The function poll will wait until one of the file descriptors passed into
		 * the function become ready to perform I/O operation. 
		 * 
		 * And in this case we are only have one file descriptor passed into poll
		 * to wait for I/O operation which is the userfaultfd passed into the thread.
		 * 
		 * We set the pollfd struct's fd field to be uffd, events to be POLLIN
		 * indicating to notify poll when there is data to read from the file descriptor
		 * 
		 * The second parameter into poll tells how many file descriptor to watch for
		 * and we only have one in this case
		 * 
		 * The last parameter tells how long to wait, -1 waits infinitely.
		 * 
		 * Poll when success return the number of file descriptor have event ready.
		 * -1 when error.
		 * 
		 * The printf statement below will print when an event occurs for the file descriptor.
		 * It & revents with the POLLIN mask and POLLERR mask to print whether it is an
		 * POLLIN event or POLLERR event occured.
		 */
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

		/* [H4]
		 * Explain following in here.
		 * If poll returns then that means it has some message either be I/O error
		 * due to POLLERR, or some message from faulting from POLLIN.
		 * 
		 * Anyway, read function read the uffd file descriptor and store the
		 * bytes read into the msg struct, this indicate a page-fault event occured and
		 * details are stored into the struct
		 * 
		 * If 0 is returned then it is EOF on the file descriptor, which I suppose cannot occur
		 * on a userdefaultfd hence it is exiting with failure.
		 * 
		 * If -1 is returned from read then it is an error on reading the file descriptor, and
		 * is exited appropriately.
		 */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		/* [H5]
		 * Explain following in here.
		 * If it is not an page fault event that we are expecting
		 * then we print the error message and exit with failure.
		 * This is because we are only looking for PAGEFAULT event
		 * with userfaultfd, if it generate some other event that we didn't
		 * tell it to look for, then something is wrong.
		 */
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* [H6]
		 * Explain following in here.
		 * If we reached here, then we indeed have a page fault event happeing
		 * the next three line print out indicating that page fault is happening.
		 * 
		 * The flag describes the event on the page fault.
		 * Because we set the mode to be UFFDIO_REGISTER_MODE_MISSING, the flag
		 * set in this field is UFFD_PAGEFAULT_FLAG_WRITE, it is indicating
		 * a write fault if UFFD_PAGEFAULT_FLAG_WRITE is set, otherwise it is
		 * a read fault.
		 * 
		 * The address is the address the page fault triggered on.
		 */
		printf("    UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* [H7]
		 * Explain following in here.
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 * Explain following in here.
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 * Explain following in here.
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 * Explain following in here.
		 */
		printf("        (uffdio_copy.copy returned %lld)\n",
                       uffdio_copy.copy);
	}
}

int
main(int argc, char *argv[])
{
	long uffd;          /* userfaultfd file descriptor */
	char *addr;         /* Start of region handled by userfaultfd */
	unsigned long len;  /* Length of region handled by userfaultfd */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int s;
	int l;

	/* [M1]
	 * Explain following in here.
	 * It checks if there is only one argument passed to the commandline
	 * If there is none, or more than one argument passed, print out the message
	 * "Usage: ./uffd num-pages" to explain that this program only take one argument
	 */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* [M2]
	 * Explain following in here.
	 * sysconf allows you to get the certain value of a configuration constants
	 * during run time. So _SC_PAGE_SIZE constant is basically asking how many
	 * bytes are in a memory page in current architecture. 
	 * 
	 * len is computed by converting the user input from either base 16, base 8, or base 10
	 * to a unsigned long and multiplied with the page_size. Len represent the total
	 * number of bytes we are mapping later. 
	 */
	page_size = sysconf(_SC_PAGE_SIZE);
	len = strtoul(argv[1], NULL, 0) * page_size;

	/* [M3]
	 * Explain following in here.
	 * Doing the syscall for userfaultfd which creates a
	 * userfaultfd object that can be used by the user program to handle
	 * page-fault rather than letting the kernel handling it by default.
	 * 
	 * The return value of the syscall is a file descriptor that refers
	 * to the userdefaultfd object. Then you can read from the userfaultfd
	 * to receive any page-fault notifications. The reading may or may not be
	 * blocking but since we specified O_NONBLOCK it will never block.
	 * 
	 * O_CLOCEXEC flag tells the file descriptor to close itself when a child
	 * is forked, because by default a forked child will inherit the parent's
	 * opened file descriptor. This flag basically says when child process does
	 * execve close the file descriptor that has O_CLOCEXEC set.
	 * 
	 * The syscall return -1 if error, otherwise is good.
	 */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	/* [M4]
	 * Explain following in here.
	 * After the userfaultfd is opened it must be enabled by calling
	 * ioctl (I/O control) to configure the file descriptor.
	 * 
	 * The first call to ioctl should take in the file descriptor as
	 * first parameter. The action code to do which is UFFDIO_API in this case,
	 * this operation just establish a handshake between kernel and the user program
	 * to determine the API version and the features supported.
	 * 
	 * The third parameter is the address of the uffdio_api struct that you have to pass into ioctl.
	 * the struct configures the api field to be UFFD_API. The feature field is set to 0
	 * and when ioctl is successful it will set this field to be a bitmask, defining
	 * what memory types are supported by the userfaultfd and what event will be generated.
	 * 
	 * Will return -1 if error basically.
	 */
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* [M5]
	 * Explain following in here.
	 * 
	 * Calls the mmap syscall to map anonymous memory into virtual machine.
	 * First parameter tells kernel where to start looking for free memory.
	 * null means let the kernel pick the address.
	 * 
	 * Since we are doing anoymous mapping we do not need to provide in
	 * the file descriptor nor the offset hence they are -1 and 0 respectively.
	 * 
	 * It is in addition private mapping therefore, each of the child processes
	 * will be inheriting the mapping through copy-on-write manner.
	 * 
	 * The protection flags specifies that the memory region can be read, and write to
	 * 
	 * And finally because this is anoymous mapping we need to provide in the
	 * number bytes which is len.
	 * 
	 * mmap will return MAP_FAILED if mapping failed and the code exits. Otherwise a pointer to the mapped
	 * area is returned and finally print the address returned by mmap
	 */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		errExit("mmap");

	printf("Address returned by mmap() = %p\n", addr);

	/* [M6]
	 * Explain following in here.
	 * ioctl with UFFDIO_REIGSTER register the anonymous page we just allocated with mmap
	 * with the userfaultfd to do our custom page-fault handling logic. 
	 * 
	 * We have to provide in uffdio_register as the struct for argp.
	 * The uffdio_register struct contain the start of the range we are monitoring
	 * the length of the range.
	 * 
	 * And the struct also specify a mode of operation, UFFDIO_REGISTER_MODE_MISSING  
	 * in this case tells userfaultfd to track page fault on missing pages for this particular
	 * memory range we have assigned.
	 * 
	 * Finally, the struct and action code UFFDIO_REIGSTER is passed into ioctl
	 * to set the fields for file descriptor uffd.
	 * 
	 * Then we just check if ioctl call return -1 for error, otherwise is successful
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	/* [M7]
	 * Explain following in here.
	 * pthread_create create a new thread. It stores the ID of the new thread into
	 * buffer thr.
	 * 
	 * The second parameter attr is null meaning the thread is created with default attribute
	 * 
	 * The third parameter specifies the function to run when this new thread is created which is
	 * fault_handler_thread in this case 
	 * 
	 * The last parameter provide the parameter for fault_handler_thread, which is the uffd file descriptor
	 * in this case. We are basically handing off the userfaultfd to the new thread we created to handle
	 * rather than the master thread.
	 */
	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}

	/*
	 * [U1]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#1. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U2]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#2. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U3]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#3. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U4]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#4. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U5]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		memset(addr+l, '@', 1024);
		printf("#5. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U6]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#6. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U7]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		memset(addr+l, '^', 1024);
		printf("#7. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U8]
	 * Briefly explain the behavior of the output that corresponds with below section.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#8. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	exit(EXIT_SUCCESS);
}
