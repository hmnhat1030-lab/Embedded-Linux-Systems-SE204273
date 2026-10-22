KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15
obj-m += asgn1_driver.o
ccflags-y += -march=armv7-a

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean
