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
	 * This is an infinite for loop.
	 * Basically this for loop will be waiting until a page-fault occurs
	 * when it occurs, it will memset the page that's allocated on this thread
	 * to be values of either all 'A' or all 'B',... all the way to 'T' depending
	 * on the number of faults occured.
	 * 
	 * Then it resolves the page-fault by copying the page filled with the bytes
	 * to the faulted address and finally printed out the number of bytes that's
	 * copied from src to destination.
	 * 
	 * It will continue this process infinitely.
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
		 * The memset below fills up the entire page allocated on the thread
		 * with characters ranging from 'A' to 'T' depending on the number of
		 * page-fault have occured. First fault it will fill it with all 'A'
		 * second fault fill with all 'B', so on and when it reach every 20th fault
		 * the byte that the buffer is filled wraps back to 'A' again.
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 * Explain following in here.
		 * uffdio_copy is the struct used to specify the memory chunk
		 * to copy into the page-faulted memory in order to resolve the page-fault
		 * 
		 * In this case, we are copying the page we have allocated on the thread
		 * that's been previously all filled with bytes specified in H7 as the src.
		 * 
		 * The dst, is the address that the page fault has occured. It is & with the negation
		 * of page_size - 1 to round the address down to be page aligned
		 * 
		 * The len specifies how many bytes to copy from page to the address, which is just
		 * page_size
		 * 
		 * mode and copy is both 0 to specify no special behavior of UFFDIO_COPY when copying
		 * like not waking up the thread that's faulted, and zero out the kernel returned place
		 * respectively.
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 * Explain following in here.
		 * This ioctl actually carries out the atomic copy in order to resolve
		 * the page fault by copying the specified page into the faulted range
		 * 
		 * 0 is returned if successful copy, -1 if error and the corresponding
		 * error is printed appropriately
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 * Explain following in here.
		 * Finally we print out the the number of bytes that's copied from
		 * src to destination, this value is returned by the kernel.
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
	 * number of bytes we are mapping later. Or in terms of pages, how many pages we are mapping
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
	 * 
	 * First, all these for loop will iterate len / 1024 times. In current machine
	 * page_size is 4096 bytes, so if only one page is mapped then for loop will go 4 times.
	 * 
	 * The FIRST ITERATION that it go through the for loop it will trigger a page-fault
	 * because the memory at addr[l] hasn't been mapped yet from virtual memory to physical memory.
	 * Our fault handler thread comes in and map 'A' as the bytes to every byte in the first page.
	 * 
	 * If there are more than one page mapped, then after every fourth iteration will trigger another page-fault
	 * So for example: If two pages are mapped, first iteration will trigger page-fault
	 * and fault handler thread resolves the fault. Second, third, and fourth iteration it won't
	 * trigger any page-fault because it is still reading the first page that's already been resolved.
	 * 
	 * But on the fifth iteration it will trigger another page fault, because the address at
	 * addr[4096] is on the second page and hasn't been mapped yet. Then it will trigger another page-fault
	 * this time the byte 'B' is mapped onto every byte of the second page.
	 * 
	 * Overall, the behavior of the following code is that depending on the number of pages mapped
	 * it will print out 4 times of the same letter per page, then increment to the next letter for
	 * the second page, and so on. After every 20th page, the letter wraps back from 'T' back to 'A' again.
	 * 
	 * The fault-handler will only trigger the number of pages times!
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
	 * The code here will print out the same print out as the previous for loop, but this time
	 * no page-fault will be triggered because it has already been resolved on the previous for loop.
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
	 * So here the syscall madvise tell the kernel to just throw out all the mappings
	 * that's done starting from address addr and extending len bytes. 
	 * 
	 * Essentially invalidating all the page-fault resolutions, and have to be resolved again.
	 * If failed to invalidate the mapping -1 is returned and program exits.
	 * 
	 * The for loop after basically does the same thing as the first for loop
	 * because we have invalidated all of the page mapping. It will trigger
	 * page-fault again on first iteration through the for loop. However, since we are
	 * continuing the lettering after the first loop it will not be mapping 'A' in the beginning
	 * but basically picking up where the first loop have left off. If first for loop
	 * stop at 'B' then the first page-fault here will pick up at 'C' assign them as each byte
	 * to the pages.
	 * 
	 * Overall, same behavior as the first for loop except the lettering will be different because
	 * it is picking off where first for loop have left off.
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
	 * Same behavior as the third for loop but without any page-fault. 
	 * Because all of the page-fault has been resolved by the third for loop.
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
	 * We again throw away the page mapping here, so we will have to page resolve again.
	 * 
	 * However, after the the page-fault has been triggered and resolved, we are
	 * overwriting over the bytes on the page to be '@'. And all of the pages will be
	 * overwritten with '@'. It just prints out '@' for each iteration each page.
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
	 * This for loop will follow the behavior as the fifth for loop but again
	 * without any page-fault because all of the page-fault resolution has already occured.
	 * And because the fifth for loop over-written all the pages with '@', this for loop
	 * will only print out '@' just like the fifth for loop.
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
	 * There is no madvise here, meaning that the page resolution we did in the fifth loop
	 * still remains, thus there is no page-fault triggered here. It will just overwrite all
	 * of the byte of the pages with '^' and print them out four '^' prints per page.
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
	 * Same behavior as the seventh for loop, because the page is still valid and contains
	 * writing from the seventh for loop it will also print out four '^' per page.
	 * Without triggering any page-fault.
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
