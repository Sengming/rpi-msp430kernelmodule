rbj-m += msp430spi.o 
obj-m += msp430spi.o 

SRC = $(shell pwd)

RPI_LATEST = /home/sengming/Documents/kernel-build/linux/

PREFIX = /home/sengming/Documents/kernel-build/tools/arm-bcm2708/arm-bcm2708-linux-gnueabi/bin/arm-bcm2708-linux-gnueabi-

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=$(PREFIX) -C $(RPI_LATEST) M=$(SRC) modules

tags: $(SRC)
	$(shell ctags -R .)


clean:
	$(MAKE) -C $(RPI_LATEST) M=$(SRC) clean

