CLIENT_OBJ = tftp.o tftp_io.o tftp_file.o logger.o tftp_def.o options.o
CLIENT_BIN = atftp

SERVER_OBJ = tftpd.o tftp_io.o tftpd_file.o tftpd_list.o logger.o \
tftp_def.o options.o stats.o
SERVER_BIN = tftpd

CARGS = -g -O3 -Wall -D_REENTRANT -DVERSION=\"atftp-0.3\"

all: atftp tftpd

%.o: %.c
	gcc $(CARGS) -c $< -o $@

atftp: $(CLIENT_OBJ)
	gcc $(CARGS) $^ -lreadline -ltermcap -o $(CLIENT_BIN)

tftpd: $(SERVER_OBJ)
	 gcc $(CARGS) $^ -lpthread -o $(SERVER_BIN)

install:
	install -o root -g root -m 0755 -d $(DESTDIR)/usr/share/man/man1 \
	$(DESTDIR)/usr/share/man/man8 $(DESTDIR)/usr/bin $(DESTDIR)/usr/sbin

	install -o root -g root -m 0644 atftp.1 $(DESTDIR)/usr/share/man/man1/atftp.1
	install -o root -g root -m 0755 $(CLIENT_BIN) $(DESTDIR)/usr/bin/$(CLIENT_BIN)
	install -o root -g root -m 0644 tftpd.8 $(DESTDIR)/usr/share/man/man8/in.tftpd.8
	install -o root -g root -m 0755 $(SERVER_BIN) $(DESTDIR)/usr/sbin/in.tftpd

clean:
	-rm -f *~ *.o $(CLIENT_BIN) $(SERVER_BIN)
