/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include "etherboot.h"


#ifdef	MOTD
/**************************************************************************
SHOW_MOTD - display the message of the day
**************************************************************************/
void show_motd(void)
{
	unsigned char *ptr;
	int  i,j,k = 0;

	for (i = 0; i < RFC1533_VENDOR_NUMOFMOTD; i++) if (motd[i]) {
		if (!k++)
			putchar('\n');
		for (j = TAG_LEN(motd[i]), ptr = motd[i]+2; j-- && *ptr; )
			putchar(*ptr++);
		putchar('\n'); }
	return;
}
#endif

#ifdef	IMAGE_MENU
#if	!defined(ANSIESC) || defined(CONSOLE_SERIAL)
static const char *const clrline = "                                                                               ";
#endif

static int getoptvalue(unsigned char **ptr, int *len, int *rc)
{
	unsigned char *tmp,*old;
	int  i,l;

	for (tmp = *ptr, l = *len; *tmp != '='; tmp++, l--);
	old = ++tmp; l--;
	if (!*tmp || *tmp == ':' || l <= 0)
		return(0);
	i = getdec(&tmp);
	if (i < 0 || (l -= tmp - old) < 0)
		return(0);
	*rc = i;
	*len = l;
	*ptr = tmp;
	return(1);
}

/**************************************************************************
PARSE_MENUOPTS - parse menu options and set control variables
**************************************************************************/
void parse_menuopts(unsigned char *opt, int len)
{
	/* This code assumes that "bootpd" terminates the control string
	   with a '\000' character */
	while (len > 0 && *opt) {
		if (!memcmp(opt,"timeout=",8)) {
			if (!getoptvalue(&opt, &len, &menutmo))
				return; }
		else if (!memcmp(opt,"default=",8)) {
			if (!getoptvalue(&opt, &len, &menudefault))
				return; }
		while (len > 0 && *opt != ':') { opt++; len--; }
		while (len > 0 && *opt == ':') { opt++; len--; }
	}
	return;
}

/**************************************************************************
GETPARMS - get user provided parameters
**************************************************************************/

#ifdef	USRPARMS
static int getparms(void)
{
	unsigned char *ptr = end_of_rfc1533+2;
	int  ch,i = 0, max = (unsigned char *)&(BOOTP_DATA_ADDR->bootp_extension[MAX_BOOTP_EXTLEN]) - ptr - 1;

	if (!end_of_rfc1533 || max < 0)
		return(0);
	else if (max > 255)
		max = 255;
restart:
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
	printf("\rParams: \033[K");
#else
	printf("%s%s%s","\rParams: ",clrline+8,"\rParams: ");
#endif
	for (ch = i; ch; )
		putchar(ptr[-(ch--)]);
	for (;;) {
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
		enable_cursor(1);
		ch = getchar();
		enable_cursor(0);
#else
		ch = getchar();
#endif
		if (ch == '\n')
			break;
		if (ch == ('U'&0x1F)) {
			ptr -= i;
			i = 0;
			goto restart; }
		if (ch == ('H'&0x1F)) {
			if (i) {
				i--; ptr--;
				printf("\010 \010"); }
			continue; }
		if (ch == ('L'&0x1F))
			goto restart;
		if (i == max || (signed char)ch < (signed char)' ')
			continue;
		putchar(*ptr++ = ch);
		i++; }
	if (i) {
		end_of_rfc1533[0] = RFC1533_VENDOR_ADDPARM;
		end_of_rfc1533[1] = i;
		*ptr++ = '\000';
		*ptr   = RFC1533_END;
		i += 3; }
	putchar('\n');
	return(i);
}
#endif

/**************************************************************************
GETHEX - compute one hex byte
**************************************************************************/
#ifdef	PASSWD
static int gethex(unsigned char *dat)
{
	int  i,j;

	i = (j = *dat) > '9' ? (j+9) & 0xF : j - '0';
	dat++;
	return(16*i + ((j = *dat) > '9' ? (j+9) & 0xF : j - '0'));
}
#endif

/**************************************************************************
SELECTIMAGE - interactively ask the user for the boot image
**************************************************************************/

/* Warning! GCC 2.7.2 has difficulties with analyzing the data flow in
   the following function; it will sometimes clobber some of the local
   variables. If you encounter strange behavior, try to introduce more
   temporary variables for storing intermediate results. This might
   help GCC... */

void selectImage(unsigned char *imagelist[])
{
#ifdef	USRPARMS
	int flag_parms = 1;
	int modifier_keys = 0;
#endif
#ifdef	PASSWD
	int flag_image = 1;
#endif
	unsigned char *e,*s,*d,*p;
	int  i,j,k,m,len;
	unsigned long tmo;
	in_addr ip;

	printf(" \nList of available boot images:\n\n");
	for (i = j = 0; i < RFC1533_VENDOR_NUMOFIMG; i++) {
		if ((e = imagelist[i]) != NULL) {
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
#ifdef	SHOW_NUMERIC
			printf("\033[4C%c) ",'1'+j++);
#else
			printf("\033[4C%c) ",'A'+j++);
#endif
#else
#ifdef SHOW_NUMERIC
			printf("    %c) ",'1'+j++);
#else
			printf("    %c) ",'A'+j++);
#endif
#endif
			for (s = e+2, len = TAG_LEN(e)+2;
			     (s - e) < len && *s != ':';
			     putchar(*s++));
			putchar('\n');
		}
	}
	m = j;
	putchar('\n');
reselect:
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
	printf("Select: \033[K");
#else
	printf("%s%s%s","\rSelect: ",clrline+8,"\rSelect: ");
#endif
	tmo = currticks()/TICKS_PER_SEC + menutmo;
	for (;;) {
		if (menutmo >= 0) { for (i = -1; !iskey(); ) {
			if ((k = tmo - currticks()/TICKS_PER_SEC) <= 0) {
selectdefault:
				if (menudefault >= 0 && menudefault <
				    RFC1533_VENDOR_NUMOFIMG) {
					i = menudefault;
					goto findimg;
				} else if (menudefault >= RFC1533_VENDOR_IMG &&
					 menudefault < RFC1533_VENDOR_IMG +
					 RFC1533_VENDOR_NUMOFIMG &&
					 imagelist[menudefault -
						  RFC1533_VENDOR_IMG]) {
					j = menudefault-RFC1533_VENDOR_IMG;
					goto img;
				}
				i = ESC;
				goto key;
			} else if (i != k) {
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
				printf("\033[s(%d seconds)\033[K\033[u",i = k);
#else
				printf("%s%s%s(%d seconds)",
				       "\rSelect: ",clrline+8,
				       "\rSelect: ",i = k);
#endif
			}
		} }
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
		enable_cursor(1);
		i = getchar();
		enable_cursor(0);
#else
		i = getchar();
#endif
	key:
#ifdef	USRPARAMS
		modifier_keys = 0;
#endif
#if	defined(USRPARMS) && defined(CONSOLE_CRT)
		modifier_keys |= getshift();
#endif
		if (i == ESC)
			longjmp(restart_etherboot, -1);
		if (i == '\n') {
			goto selectdefault;
		}
		if (i == '\t' || i == ' ') {
			menutmo = -1;
			printf("\r");
			goto reselect;
		}
		if ((i >= 'A') && (i < 'A'+m)) {
			i -= 'A';
#ifdef	USRPARMS
			modifier_keys |= 3; /* ShiftL + ShiftR */
#endif
			break;
		} else if ((i >= 'a') && (i < 'a'+m)) {
			i -= 'a';
			break;
		} else if ((i >= '1') && (i < '1'+m)) {
			i -= '1';
			break;
		}
	}
findimg:
	j = 0;
	while (i >= 0) {
		if (imagelist[j] != NULL) i--;
		j++;
	}
	j--;
img:
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
	printf("\033[K");
#else
	printf("%s%s%s","\rSelect: ",clrline+8,"\rSelect: ");
#endif
	for (len = TAG_LEN(e = imagelist[j]) + 2, s = e + 2;
	     (s - e) < len && *s != ':';
	     putchar(*s++));
	putchar('\n');
	for (i = ARP_SERVER; i <= ARP_GATEWAY; i++) {
		if ((++s - e) >= len) goto local_disk;
		if (inet_aton(s, &ip)) {
			arptable[i].ipaddr.s_addr = ip.s_addr;
			memset(arptable[i].node, 0, ETH_ALEN);
			while ((s - e) < len && *s != ':') s++;
		}
	}
	if ((++s - e) >= len) goto local_disk;
	for (d = s; (d - e) < len && *d != ':'; d++);
	for (p = d + 1, j = 0; (p - e) < len && *p && *p != ':'; p++, j++);
	for (p++, i = 0; (p - e) < len && *p != ':'; p++) {
#if	defined(USRPARMS) || defined(PASSWD)
		if (*p >= '0' && *p <= '9')
			i = 10*i + *p - '0';
		else {
			k = *p & ~0x20;
#ifdef	USRPARMS
			if (k == 'P') {
				flag_parms = i;
				/* 0 - never interactively accept parameters
				 * 1 - require password before accepting parms.
				 * 2 - always ask for passwd and parameters
				 * 3 - always ask for parameters
				 */
			}
#endif
#ifdef	PASSWD
			if (k == 'I') {
				flag_image = i;
				/* 0 - do not require a password for this image
				 * 1 - require a password for this image
				 */
			}
#endif
			i = 0;
		}
#endif
	}
	defparams = p + 1;
	defparams_max = len - (defparams - (unsigned char *)e);
#ifdef	USRPARMS
	if (flag_parms == 1 && modifier_keys)
		flag_parms++;
#endif
#ifdef	PASSWD
	if ((flag_image > 0
#ifdef	USRPARMS
	     || flag_parms == 2
#endif
	    ) && j == 32) {
		unsigned char md5[16];
		printf("Passwd: ");
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
		enable_cursor(1);
#endif
		while ((i = getchar()) != '\n') {
			if (i == ('U'&0x1F)) md5_done(md5);
			else                 md5_put(i);
		}
#if	defined(ANSIESC) && defined(CONSOLE_CRT)
		enable_cursor(0);
#endif
		md5_done(md5);
		for (i = 16, p = d+31; i--; p -= 2) {
			if (gethex(p) != md5[i]) {
#if	defined(ANSIESC) && !defined(CONSOLE_SERIAL)
				printf("\r\033[K\033[1A");
#else
				printf("\r");
#endif
				goto reselect;
			}
		}
		putchar('\n');
	}
#endif
#ifdef	USRPARMS
	if (flag_parms > 1)
		i = getparms();
	else
		i = 0;
#endif
	if ((len = d - s) <= 0 || len > (int)sizeof(KERNEL_BUF)-1 || !*s) {
	local_disk:
		cleanup();
		eb_exit(0);
	}


	if (end_of_rfc1533 != NULL &&
	    (end_of_rfc1533+4) < (unsigned char *)&(BOOTP_DATA_ADDR->bootp_extension[MAX_BOOTP_EXTLEN])) {
#ifdef	USRPARMS
		d    = end_of_rfc1533 + i;
#else
		d    = end_of_rfc1533;
#endif
		i    = e-d;
		*d++ = RFC1533_VENDOR_SELECTION;
		*d++ = 1;
		*d++ = *e;
		*d   = RFC1533_END;
	}

	/* if name is -, reuse previous filename */
        if (!(s[0] == '-' && s[1] == ':')) {
            memcpy(KERNEL_BUF, s, len);
            KERNEL_BUF[len] = '\0';
        }
	return;
}
#endif

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
