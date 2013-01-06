OBJS=globalFunctions.o lists.o omxVideo.o demuxer.o connection.o mythProtocol.o osd.o  bcm.o vcos.o tvservice.o client.o
BIN=piMythClient
LDFLAGS+=-luuid -lavformat -lavcodec -lavutil -pthread -lz -lx264 -laacplus -lm -lbz2 -lrt
CFLAGS+=-Wall 
INCLUDES+=


all: $(OBJS) $(LIB)
	$(CC) -o $(BIN) -Wl,--whole-archive $(OBJS) -Wl,--no-whole-archive $(LDFLAGS) -rdynamic

%.o: %.c
	@rm -f $@ 
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

%.a: $(OBJS)
	$(AR) r $@ $^

debug: CFLAGS += -DDEBUG
debug: all

debug-demuxer: CFLAGS += -DDEBUG_DEMUXER
debug-demuxer: all

pi: CFLAGS+=-DPI -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -mstructure-size-boundary=32 -mfloat-abi=hard -fomit-frame-pointer -mabi=aapcs-linux -mfpu=vfp
pi: LDFLAGS+=-L$(SDKSTAGE)/opt/vc/lib/ -L/lib -L/usr/lib/arm-linux-gnueabihf/ -lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -L/opt/vc/src/hello_pi/libs/ilclient -L/opt/vc/src/hello_pi/libs/vgfont -lvgfont -lfreetype -lz
pi: INCLUDES+=-I$(SDKSTAGE)/opt/vc/include/ -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads -I./ -I/opt/vc/src/hello_pi/libs/ilclient -I/opt/vc/src/hello_pi/libs/vgfont -I/include
pi: all

pi-debug: CFLAGS += -DDEBUG
pi-debug: pi

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(BIN) $(LIB)
