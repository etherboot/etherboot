# Comment out the second command and uncomment the first command
# below if you don't want to use libwrap (hosts.{allow,deny} access control)

LIBWRAP=-lwrap

p910nd:	p910nd.c
#	$(CC) -Wall -o $@ p910nd.c
	$(CC) -Wall -DUSE_LIBWRAP -o $@ p910nd.c $(LIBWRAP)
