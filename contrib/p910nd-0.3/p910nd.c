/*
 *	Port 9100+n daemon
 *	Accepts a connection from port 9100+n and copy stream to
 *	/dev/lpn, where n = 0,1,2.
 *
 *	Run standalone as: p910nd [0|1|2]
 *
 *	Run under inetd as:
 *	p910n stream tcp nowait root /usr/sbin/tcpd p910nd [0|1|2]
 *	 where p910n is an /etc/services entry for
 *	 port 9100, 9101 or 9102 as the case may be.
 *	 root can be replaced by any uid with rw permission on /dev/lpn
 *
 *	Port 9100+n will then be passively opened
 *	n defaults to 1 because most Linux PCs have the first port at 0x378
 *
 *	Version 0.3
 *	Ken Yap (ken_yap@users.sourceforge.net), November 1999
 *	Placed under GPL.
 *
 *	Added locking so that two invocations of the daemon under inetd
 *	don't try to open the printer at the same time. This can happen
 *	even if there is one host running clients because the previous
 *	client can exit after it has sent all data but the printer has not
 *	finished printing and inetd starts up a new daemon when the next
 *	request comes in too soon.
 *
 *	Various things are Linux or even RedHat specific. I don't
 *	think there is much demand for this program outside of PCs,
 *	but if you port it to other distributions or platforms,
 *	I'd be happy to receive your patches.
 */

#include	<unistd.h>
#include	<stdio.h>
#include	<string.h>
#include	<fcntl.h>
#include	<netdb.h>
#include	<syslog.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>

#define		BASEPORT	9100
#define		PIDFILE		"/var/run/p910%cd.pid"
#define		LOCKFILE	"/var/lock/subsys/p910%cd"
#define		PRINTERFILE	"/dev/lp%c"
#define		LOGOPTS		(LOG_PERROR|LOG_PID|LOG_LPR|LOG_ERR)

static char	*progname;
static char	version[] = "p910nd Version 0.3";
static int	lockfd = -1;

void usage(void)
{
	fprintf(stderr, "Usage: %s [0|1|2]\n", progname);
	exit(1);
}

FILE *open_printer(int lpnumber)
{
	FILE		*f;
	char		lpname[sizeof(PRINTERFILE)];

#ifndef	TESTING
	(void)sprintf(lpname, PRINTERFILE, lpnumber);
#else
	(void)strcpy(lpname, "printer");
#endif
	if ((f = fopen(lpname, "w")) == NULL)
	{
		syslog(LOGOPTS, "%s: %m\n", lpname);
		exit(1);
	}
	return (f);
}

int get_lock(int lpnumber)
{
	char		lockname[sizeof(LOCKFILE)];
	struct flock	lplock;

	(void)sprintf(lockname, LOCKFILE, lpnumber);
	if ((lockfd = open(lockname, O_CREAT|O_RDWR)) < 0)
	{
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	memset(&lplock, 0, sizeof(lplock));
	lplock.l_type = F_WRLCK;
	lplock.l_pid = getpid();
	if (fcntl(lockfd, F_SETLKW, &lplock) < 0)
	{
		syslog(LOGOPTS, "%s: %m\n", lockname);
		return (0);
	}
	return (1);
}

void free_lock(void)
{
	if (lockfd >= 0)
		(void)close(lockfd);
}

/* Copy network socket to FILE f until EOS */
int copy_stream(int fd, FILE *f)
{
	int		nread;
	char		buffer[8192];

	while ((nread = read(fd, buffer, sizeof(buffer))) > 0)
		(void)fwrite(buffer, sizeof(char), nread, f);
	(void)fflush(f);
	return (nread);
}

void one_job(int lpnumber)
{
	FILE		*f;
	struct sockaddr_in	client;
	int		clientlen = sizeof(client);

	if (getpeername(0, &client, &clientlen) >= 0)
		syslog(LOGOPTS, "Connection from %s port %hd\n",
			inet_ntoa(client.sin_addr),
			ntohs(client.sin_port));
	if (get_lock(lpnumber) == 0)
		return;
	f = open_printer(lpnumber);
	if (copy_stream(0, f) < 0)
		syslog(LOGOPTS, "read: %m\n");
	fclose(f);
	free_lock();
}

void server(int lpnumber)
{
	struct rlimit	resourcelimit;
	struct protoent	*proto;
	int		netfd, fd, clientlen, one = 1;
	struct sockaddr_in	netaddr, client;
	char		pidfilename[sizeof(PIDFILE)];
	FILE		*f;

#ifndef	TESTING
	switch (fork())
	{
	case -1:
		syslog(LOGOPTS, "fork: %m\n");
		exit (1);
	case 0:		/* child */
		break;
	default:	/* parent */
		exit(0);
	}
	/* Now in child process */
	resourcelimit.rlim_max = 0;
	if (getrlimit(RLIMIT_NOFILE, &resourcelimit) < 0)
	{
		syslog(LOGOPTS, "getrlimit: %m\n");
		exit(1);
	}
	for (fd = 0; fd < resourcelimit.rlim_max; ++fd)
		(void)close(fd);
	if (setsid() < 0)
	{
		syslog(LOGOPTS, "setsid: %m\n");
		exit(1);
	}
	(void)chdir("/");
	(void)umask(022);
	fd = open("/dev/null", O_RDWR);	/* stdin */
	(void)dup(fd);			/* stdout */
	(void)dup(fd);			/* stderr */
	(void)sprintf(pidfilename, PIDFILE, lpnumber);
	if ((f = fopen(pidfilename, "w")) == NULL)
	{
		syslog(LOGOPTS, "%s: %m\n", PIDFILE);
		exit(1);
	}
	(void)fprintf(f, "%d\n", getpid());
	(void)fclose(f);
	if (get_lock(lpnumber) == 0)
		exit(1);
#endif
	f = open_printer(lpnumber);
	if ((proto = getprotobyname("tcp")) == NULL)
	{
		syslog(LOGOPTS, "Cannot find protocol for TCP!\n");
		exit(1);
	}
	if ((netfd = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0)
	{
		syslog(LOGOPTS, "socket: %m\n");
		exit(1);
	}
	if (setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
	{
		syslog(LOGOPTS, "setsocketopt: %m\n");
		exit(1);
	}
	netaddr.sin_port = htons(BASEPORT + lpnumber - '0');
	netaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(netaddr.sin_zero, 0, sizeof(netaddr.sin_zero));
	if (bind(netfd, &netaddr, sizeof(netaddr)) < 0)
	{
		syslog(LOGOPTS, "bind: %m\n");
		exit(1);
	}
	if (listen(netfd, 5) < 0)
	{
		syslog(LOGOPTS, "listen: %m\n");
		exit(1);
	}
	clientlen = sizeof(client);
	memset(&client, 0, sizeof(client));
	while ((fd = accept(netfd, &client, &clientlen)) >= 0)
	{
		syslog(LOGOPTS, "Connection from %s port %hd\n",
			inet_ntoa(client.sin_addr),
			ntohs(client.sin_port));
		/*write(fd, "Printing", 8);*/
		if (copy_stream(fd, f) < 0)
			syslog(LOGOPTS, "read: %m\n");
		(void)close(fd);
	}
	syslog(LOGOPTS, "accept: %m\n");
	free_lock();
	exit(1);
}

int is_standalone(void)
{
	struct sockaddr_in	bind_addr;
	int			ba_len;

	/*
	 * Check to see if a socket was passed to us from inetd.
	 *
	 * Use getsockname() to determine if descriptor 0 is indeed a socket
	 * (and thus we are probably a child of inetd) or if it is instead
	 * something else and we are running standalone.
	 */
	ba_len = sizeof(bind_addr);
	if (getsockname(0, &bind_addr, &ba_len) == 0)
		return (0);		/* under inetd */
	if (errno != ENOTSOCK)		/* strange... */
		syslog(LOGOPTS, "getsockname: %m\n");
	return (1);
}

int main(int argc, char *argv[])
{
	int		lpnumber;
	char		*p;

	if (argc <= 0)		/* in case not provided in inetd.conf */
		progname = "p910nd";
	else
	{
		progname = argv[0];
		if ((p = strrchr(progname, '/')) != 0)
			progname = p + 1;
	}
	lpnumber = '1';
	if (argc > 1)
	{
		if (argv[1][1] != '\0')
			usage();
		switch (argv[1][0])
		{
		case '0': case '1': case '2':
			lpnumber = argv[1][0];
			break;
		default:
			usage();
			break;
		}
	}
	/* change the n in argv[0] to match the port so ps will show that */
	if ((p = strstr(argv[0], "p910n")) != NULL)
		p[4] = lpnumber;
	if (is_standalone())
		server(lpnumber);
	else
		one_job(lpnumber);
	return (0);
}
