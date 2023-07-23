ver = debug
platform := x64

# Raspberry Pi in 32-bit LE mode
ifeq ($(platform), armv7l)
platform := armv7
endif

ifeq ($(platform), aarch64)
platform := armv8
endif

CC = g++

ifeq ($(ver), debug)
DEFS = -D_LIN -D_DEBUG
CFLAGS = -g  -I $(INCLIB) $(DEFS) -lpthread  -DGLIBC_20
else
DEFS = -D_LIN
CFLAGS =  -O3 -I $(INCLIB) $(DEFS) -lpthread  -DGLIBC_20
endif

ifeq ($(platform), x64)
CFLAGS += -m64
CFLAGS += -lrt
endif

#CFLAGS += -L./sdk/lib/$(platform) -I./sdk/include -pedantic -Wimplicit-fallthrough -Wall -Werror -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio
CFLAGS += -L../lib/$(platform) -I../include -pedantic -Wimplicit-fallthrough -Wall -Werror -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio
LIBSPATH = -L../lib/$(platform) -I../include -I/usr/include/opencv4


zwostream2RMS: main.cpp Makefile
	$(CC) main.cpp -o zwostream2RMS $(CFLAGS) -lASICamera2 $(LIBSPATH)

clean:
	rm -f zwostream2RMS

#pkg-config libusb-1.0 --cflags --libs
#pkg-config opencv --cflags --libs
