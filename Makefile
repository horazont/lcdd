LIBS_STROPHE_FB=../libstrophe/libstrophe.a
LIBS_STROPHE_LOCAL=../libstrophe-local/libstrophe.a

LIBS_FRITZBOX=$(LIBS_STROPHE_FB) ../fritzbox-libs/libssl.so ../fritzbox-libs/libcrypto.so.1 ../fritzbox-libs/libc.so ../fritzbox-libs/libexpat.so

LD_LOCAL=

CFLAGS_STROPHE_FB=-I../libstrophe/
CFLAGS_STROPHE_LOCAL=-I../libstrophe-local/

CFLAGS=-Wall -Werror -std=gnu99

FBCC=mips-linux-gcc $(CFLAGS_STROPHE_FB) $(CFLAGS) -Os
LOCC=gcc $(CFLAGS_STROPHE_LOCAL) $(CFLAGS) -g -O2

LOCAL_OBJS=objs/utils-local.o objs/xmpp-utils-local.o objs/commands-local.o objs/display-local.o
FB_OBJS=objs/utils.o objs/xmpp-utils.o objs/commands.o objs/display.o


lcdd-local: lcdd.c $(LOCAL_OBJS)
	$(LOCC) $(CFLAGS_STROPHE_LOCAL) -lssl -lcrypto -lexpat -lresolv -o lcdd-local lcdd.c $(LOCAL_OBJS) $(LIBS_STROPHE_LOCAL)

lcdd: lcdd.c $(FB_OBJS)
	$(FBCC) -o lcdd lcdd.c $(FB_OBJS) $(CFLAGS_STROPHE_FB) $(LIBS_FRITZBOX)

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

