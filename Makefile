LIBS_COUPLET_FB=../libcouplet/.libs/libcouplet.a
LIBS_COUPLET_LOCAL=../libcouplet-local/.libs/libcouplet.a

LIBS_FRITZBOX=$(LIBS_COUPLET_FB) ../fritzbox-libs/libssl.so ../fritzbox-libs/libcrypto.so.1 ../fritzbox-libs/libc.so ../fritzbox-libs/libexpat.so ../fritzbox-libs/libpthread.so

LD_LOCAL=

CFLAGS_COUPLET_FB=-I../libcouplet/include/
CFLAGS_COUPLET_LOCAL=-I../libcouplet-local/include/

CFLAGS=-Wall -Werror -std=gnu99

FBCC=mips-linux-gcc $(CFLAGS_COUPLET_FB) $(CFLAGS) -Os
LOCC=gcc $(CFLAGS_COUPLET_LOCAL) $(CFLAGS) -g -O2

LOCAL_OBJS=objs/utils-local.o objs/xmpp-utils-local.o objs/commands-local.o objs/display-local.o objs/config-local.o
FB_OBJS=objs/utils.o objs/xmpp-utils.o objs/commands.o objs/display.o objs/config.o


lcdd-local: lcdd.c $(LOCAL_OBJS)
	$(LOCC) $(CFLAGS_COUPLET_LOCAL) -lrt -lssl -lcrypto -lexpat -lresolv -lpthread -o lcdd-local lcdd.c $(LOCAL_OBJS) $(LIBS_COUPLET_LOCAL)

lcdd: lcdd.c $(FB_OBJS)
	$(FBCC) -o lcdd lcdd.c $(FB_OBJS) $(CFLAGS_COUPLET_FB) $(LIBS_FRITZBOX)

objs/utils-local.o: utils.c utils.h common.h
	$(LOCC) -c -o $@ utils.c

objs/utils.o: utils.c utils.h common.h
	$(FBCC) -c -o $@ utils.c

objs/xmpp-utils-local.o: xmpp-utils.c xmpp-utils.h utils.h common.h
	$(LOCC) -c -o $@ xmpp-utils.c

objs/xmpp-utils.o: xmpp-utils.c xmpp-utils.h utils.h common.h
	$(FBCC) -c -o $@ xmpp-utils.c

objs/commands-local.o: commands.c commands.h utils.h common.h display.h
	$(LOCC) -c -o $@ commands.c

objs/commands.o: commands.c commands.h utils.h common.h display.h
	$(FBCC) -c -o $@ commands.c

objs/display-local.o: display.c display.h utils.h common.h
	$(LOCC) -c -o $@ display.c

objs/display.o: display.c display.h utils.h common.h
	$(FBCC) -c -o $@ display.c

objs/config-local.o: config.c config.h common.h
	$(LOCC) -c -o $@ config.c

objs/config.o: config.c config.h common.h
	$(FBCC) -c -o $@ config.c
