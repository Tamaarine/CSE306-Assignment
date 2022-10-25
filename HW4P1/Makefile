CC=gcc
LOCAL_CFLAGS=-Wall -Werror

all: perftop.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: perftop.ko
	sudo insmod $< int_str='"11 44 22 33 5"'

uninstall: perftop.ko
	sudo rmmod $<
