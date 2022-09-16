CC=gcc
LOCAL_CFLAGS=-Wall -Werror

obj-m += kds.o

all: kds.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: kds.ko
	sudo insmod $< int_str='"         2               3"'

uninstall: kds.ko
	sudo rmmod $<

