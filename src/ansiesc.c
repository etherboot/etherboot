#include "etherboot.h"

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#if	defined(ANSIESC) && defined(CONSOLE_CRT)

/*
 *	Display attributes
 *
 *	Code          Effect
 *	<esc>[0m      normal text
 *	<esc>[1m      high-intensity on
 *	<esc>[21m     high-intensity off
 *	<esc>[5m      blinking on
 *	<esc>[25m     blinking off
 *	<esc>[7m      reverse video on
 *	<esc>[27m     reverse video off
 *	<esc>[3xm     set foreground color:
 *	<esc>[4xm     set background color. x can be:
 *			   0 - black                   4 - blue
 *			   1 - red                     5 - magenta
 *			   2 - green                   6 - cyan
 *			   3 - yellow                  7 - white
 *	<esc>[=xh     set video mode
 *			   0 - 40x25 mono     (text)  13 - 40x25 16colors (gfx)
 *			   1 - 40x25 16colors (text)  14 - 80x25 16colors (gfx)
 *			   2 - 80x25 mono     (text)  15 - 80x25 mono     (gfx)
 *			   3 - 80x25 16colors (text)  16 - 80x25 16colors (gfx)
 *			   4 - 40x25 4colors  (gfx)   17 - 80x30 mono     (gfx)
 *			   5 - 40x25 mono     (gfx)   18 - 80x30 16colors (gfx)
 *			   6 - 80x25 mono     (gfx)   19 - 40x25 256colors(gfx)
 *
 *
 *	Cursor control
 *
 *	Code          Effect
 *	<esc>[r;cH    move cursor to row r col c (r and c are both numbers)
 *	<esc>[r;cf    move cursor to row r col c (r and c are both numbers)
 *	<esc>[rA      move cursor up r rows
 *	<esc>[rB      move cursor down r rows
 *	<esc>[cC      move cursor right c columns
 *	<esc>[cD      move cursor left c columns
 *	<esc>[?7l     turn off line wrap
 *	<esc>[?7h     turn on line wrap
 *	<esc>[J       clear screen and home cursor
 *	<esc>[K       clear to end of line
 *	<esc>[s       save the cursor position
 *	<esc>[u       return cursor to saved position
 *
 *	Extended features
 *
 *	Code          Effect
 *	<esc>['filename'#
 *	              load include file with TFTP or NFS
 *      <esc>[a;b;c;d+<data>
 *                    draw pixel data; use one byte per pixel. Parameters
 *                    differ depending on the number of parameters passed:
 *                    cnt
 *                      "cnt" bytes follow; they will be drawn to the
 *                      right of the last graphics position
 *                    rle;col
 *                      the next "rle" pixels have the value "col"; they
 *                      will be drawn to the right of the last graphics
 *                      position
 *                    x;y;cnt
 *                      "cnt" bytes follow; they will be drawn relative to
 *                      the text cursor position with an offset of (x/y)
 *                    x;y;rle;col
 *                      the next "rle" pixels have the value "col"; the
 *                      will be drawn relative to the text cursor position
 *                      with an offset of (x/y)
 *      <esc>[a;b;c;d-<data>
 *                      same as above, but pack pixels into three bits.
 *
 */

#define MAXARGS		8
#define MAXSTRARGS	1
#define MAXSTRARGLEN	40

#ifdef	ETHERBOOT32
extern union {
  struct {
    unsigned char al,ah,bl,bh,cl,ch,dl,dh;
  } __attribute__ ((packed)) lh;
  struct {
    unsigned short ax,bx,cx,dx;
  } __attribute__ ((packed)) x;
} __attribute__ ((packed)) int10ret;
#endif
#ifdef	ETHERBOOT16
extern union {
	struct {
		unsigned char	al, ah, bl, bh, cl, ch, dl, dh;
	} lh;
	struct {
		unsigned short	ax, bx, cx, dx;
	} x;
} int10ret;
#endif

#define int10(a,b,c,d) _int10((unsigned long)(a)+((unsigned long)(b)<<16), \
			      (unsigned long)(c)+((unsigned long)(d)<<16))
  extern void _int10(unsigned long axbx,unsigned long cxdx);

static enum { esc_init, esc_std, esc_esc, esc_bracket, esc_digit,
	      esc_semicolon, esc_str, esc_quote
} esc_state = esc_init;

static unsigned short scr,rows,columns,attr = 7,cx = 0,cy = 0, scx = 0,scy = 0;
static int fg = 7, bg = 0, blink = 0,reverse = 0, bright = 0;
static int wraparound = 1;
static int argn = 0;
static int argi[MAXARGS];
static char args[MAXSTRARGS][MAXSTRARGLEN];
static int in_download = 0;
static const char coltable[9] = "\000\004\002\006\001\005\003\007";
#ifdef	GFX
static unsigned short gfx_rows,gfx_columns,char_width,char_height,gfx_x,gfx_y;
static int in_gfx = 0, gfx_packed,gfx_data,gfx_nbits;
static int defaultmode = -1, curmode = -1, curmodetype = 0;
#endif


void ansi_reset(void)
{
#ifdef	GFX
  /* this will not execute, when the serial console is used, because
     both curmode and defaultmode will be -1 */
  if (curmode != defaultmode) {
    in_download = in_gfx = 0;
    curmodetype = 0;
    int10(curmode = defaultmode,0,0,0);
    esc_state = esc_init;
    handleansi('\010'); /* force reinitialization */ }
  return;
#endif
}

void enable_cursor(int enable)
{
#ifdef	GFX
  /* this will not execute, when the serial console is used, because
     curmodetype will be 0 */
  if (curmodetype == 'G' || curmodetype == 'H') {
    if (enable)
      int10(0x09DB,7,1,0);
    else
      int10(0x0920,(attr/16)&7,1,0); }
  return;
#endif
}

static int downloadmotd(unsigned char *data,int block,int len,int eof)
{
  in_download = -1;
  while (len--)
    handleansi(*data++);
  in_download = 1;
  return(eof ? 1 : -1);
}

static void docommand(unsigned char ch)
{
  if (ch == '#') {      /* include file */
    if (!in_download) {
      in_download = 1;
      download(args[0],downloadmotd);
      in_download = 0; } }
#ifdef	GFX
  else if (ch == '+' || ch == '-') { /* gfx bitmap */
    int i;
    gfx_packed = ((ch == '-') ? 3 : 8);
    gfx_nbits  = 0;
    if (argn > 2) {
      gfx_x    = argi[0];
      gfx_y    = argi[1];
      if (curmodetype == 'T') {
	gfx_x += cx;
	gfx_y += cy; }
      else {
	gfx_x += cx*char_width;
	gfx_y += cy*char_height; }
      i        = 2; }
    else
      i        = 0;
    in_gfx   = argi[i+0];
    if (argn == i+2)
      while (in_gfx)
	handleansi(argi[i+1]); }
#endif
  else if (ch == 'm') { /* set attribute */
    int i,j;
    for (j = 0; (i = argi[j]), (j++ < argn); )
      if (i == 0 ||       /* reset attributes */
	  i == 20) {
	blink = reverse = bright = bg = 0; fg = 7;
	goto doattr; }
      else if (i == 1) {  /* high intensity on */
	bright = 1;
	goto doattr; }
      else if (i == 21) { /* high intensity off */
	bright = 0;
	goto doattr; }
      else if (i == 5) {  /* blinking on */
	blink = 1;
	goto doattr; }
      else if (i == 25) { /* blinking off */
	blink = 0;
	goto doattr; }
      else if (i == 7) {  /* reverse video on */
	reverse = 1;
      isreverse:
	attr =
#ifdef	GFX
	  curmodetype != 'T' ? (fg?128:0)+16*fg+(fg^(8*bright+bg)) :
#endif
	  128*blink+16*fg+8*bright+bg; }
      else if (i == 27) { /* reverse video off */
	reverse = 0;
      isnormal:
	attr =
#ifdef	GFX
	  curmodetype != 'T' ? (bg?128:0)+16*bg+(bg^(8*bright+fg)) :
#endif
	  128*blink+16*bg+8*bright+fg; }
      else if (i >= 30 && i <= 37) {
	fg = coltable[i-30];
      doattr:
	if (reverse) goto isreverse; else goto isnormal; }
      else if (i >= 40 && i <= 47) {
	bg = coltable[i-40];
	goto doattr; } }
  else if (ch == 'H' || /* set cursor position */
	   ch == 'f') { /* set cursor position */
    cy = argi[0];
    cx = argi[1];
    updatecursor:
    int10(0x0200,256*scr,0,256*cy+cx); }
  else if (ch == 'A') { /* move cursor up */
    if (cy < argi[0]) cy = 0;
    else              cy -= argi[0];
    goto updatecursor; }
  else if (ch == 'B') { /* move cursor down */
    if ((cy += argi[0]) >= rows) cy = rows-1;
    goto updatecursor; }
  else if (ch == 'C') { /* move cursor right */
    if ((cx += argi[0]) >= columns) cx = columns-1;
    goto updatecursor; }
  else if (ch == 'D') { /* move cursor left */
    if (cx < argi[0]) cx = 0;
    else              cx -= argi[0];
    goto updatecursor; }
  else if (ch == 'l') { /* turn off line wrapping or set text mode */
    if (argi[0] == 7)
      wraparound = 0;
#ifdef	GFX
    else {  /* text mode */
      curmodetype = 0;
      int10(curmode = defaultmode,0,0,0);
      esc_state = esc_init; }
#endif
  }
  else if (ch == 'h') { /* turn on line wrapping or set graphics mode */
    if (argi[0] == 7)
      wraparound = 1;
#ifdef	GFX
    else {  /* graphics mode */
      curmodetype = 0;
      int10(curmode = argi[0],0,0,0);
      esc_state = esc_init; }
#endif
  }
  else if (ch == 'J') { /* clear screen and home cursor */
#ifdef	GFX
    int10(0x0600,256*(curmodetype != 'T' ? reverse ? fg : bg : attr),0,
	  256*(rows-1)+columns-1);
#else
    int10(0x0600,256*attr,0,256*(rows-1)+columns-1);
#endif
    cx = cy = 0;
    goto updatecursor; }
  else if (ch == 'K')   /* clear to end of line */
#ifdef	GFX
    int10(curmodetype == 'T' ? 0x0920 : 0x09DB,
	  curmodetype == 'T' ? 256*scr+attr : reverse ? fg : bg,
	  columns-cx,0);
#else
    int10(0x0920,256*scr+attr,columns-cx,0);
#endif
  else if (ch == 's') { /* save cursor position */
    scx = cx; scy = cy; }
  else if (ch == 'u') { /* restore cursor position */
    cx = scx; cy = scy;
    goto updatecursor; }
  return;
}

void handleansi(unsigned char ch)
{
  if (in_download == 1)
    return;
#ifdef	GFX
  if (in_gfx) {
    gfx_data   = (gfx_data << 8) | ch;
    gfx_nbits += 8;
    while (gfx_nbits >= gfx_packed && in_gfx) {
      in_gfx--;
      ch = (gfx_data >> (gfx_nbits -= gfx_packed)) & ((1 << gfx_packed) - 1);
      if (curmodetype == 'T') {
	if (gfx_x < columns && gfx_y < rows*2) {
	  int pix[2];
	  int10(0x0200,256*scr,0,        /* move cursor position */
		256*(gfx_y/2)+gfx_x++);
	  int10(0x0800,256*scr,0,0);     /* read character/attribute */
	  if (int10ret.lh.al == 0xDB)
	    pix[0] = pix[1] =  int10ret.lh.ah    &0xF;
	  else if (int10ret.lh.al == 0xDC) {
	    pix[0] =          (int10ret.lh.ah/16)&0xF;
	    pix[1] =           int10ret.lh.ah    &0xF; }
	  else
	    pix[0] = pix[1] = (int10ret.lh.ah/16)&0xF;
	  pix[gfx_y&1] = coltable[ch & 0x7];
	  int10(0x0900|                  /* output char */
		(pix[0]==pix[1]?0xDB:0xDC),
		256*scr+(0x7F&(pix[0]*16+pix[1])),1,0); }
	if (!in_gfx)
	  int10(0x0200,256*scr,0,        /* restore cursor position */
		256*cy+cx); }
      else if (gfx_x < gfx_columns && gfx_y < gfx_rows)
	int10(0x0C00|                    /* write pixel */
	      (ch <= 7 ? coltable[ch] : ch),256*scr,gfx_x++,gfx_y); }
    return; }
#endif
  switch (esc_state) {
  case esc_esc:
    if (ch == '[') {
      esc_state = esc_bracket;
      argn = 0;
      memset(&argi, 0, sizeof(argi));
      memset(args, 0, MAXSTRARGS*MAXSTRARGLEN); }
    else {
      esc_state = esc_std;
      goto doputchar; }
    break;
  case esc_bracket:
  case esc_semicolon:
    if (ch == '\'') {
      esc_state = esc_str;
      break; }
    /* fall thru */
    if (ch == '=' || ch == '?')
      break;
    /* fall thru */
  case esc_digit:
    if (ch >= '0' && ch <= '9') {
      esc_state = esc_digit;
      if (argn < MAXARGS)
	argi[argn] = 10*argi[argn] + ch - '0'; }
    else
      quote:
      if (ch == ';') {
      esc_state = esc_semicolon;
      argn++; }
    else {
      if (esc_state == esc_digit || esc_state == esc_quote)
	argn++;
      esc_state = esc_std;
      docommand(ch); }
    break;
  case esc_str:
    if (ch == '\'') {
      esc_state = esc_quote;
      break; }
    { int i;
    if (argn < MAXSTRARGS && (i = strlen(args[argn])) < MAXSTRARGLEN-2)
      args[argn][i] = ch; }
    break;
  case esc_quote:
    goto quote;
  case esc_init: {
    int i;
    esc_state = esc_std;
    int10(0x0F00,0,0,0);                 /* get graphics mode information */
#ifdef	GFX
    if (defaultmode < 0)
      defaultmode = curmode = int10ret.lh.al ? int10ret.lh.al : 3;
#endif
    columns = int10ret.lh.ah;
    scr     = int10ret.lh.bh;
    int10(0x0200,256*scr,0,0);           /* put cursor to home position */
    for (i = 0; ++i < 100; ) {
      int10(0x0E0A,256*scr,0,0);         /* output LF */
      int10(0x0300,256*scr,0,0);         /* get cursor position */
      if (int10ret.lh.dh != i)	         /* row */
	break; }
    rows    = i;
    int10(0x0200,256*scr,0,0);           /* put cursor to home position */
#ifdef	GFX
    char_width  = 8;
    char_height = *(unsigned char *)0x485;
    if (char_height < 8 || char_height > 20)
      char_height = 8;
    gfx_columns = char_width*columns;
    gfx_rows    = char_height*rows;
    if (!curmodetype) {
      int10(0x0941,256*scr+127,1,0);     /* 'A', bright, white on white */
      int10(0x0800,256*scr,0,0);         /* read character/attribute */
      if (int10ret.lh.al == 0x41 &&
	  int10ret.lh.ah == 127)
	curmodetype = 'T';
      else {
	int10(0x0C40,256*scr,0,0);       /* write pixel */
	int10(0x0D00,256*scr,0,0);       /* read pixel color */
	if (int10ret.lh.al == 0x40)
	  curmodetype = 'H';
	else
	  curmodetype = 'G'; } }
#endif
    attr = fg = 7;
    cx = cy = scx = scy = bg = blink = reverse = bright = 0;
    wraparound = 1;
#ifdef	GFX
    int10(0x0600,                        /* clear screen */
	  curmodetype != 'T' ? 0 : 7*256,0,256*(rows-1)+columns-1); }
#else
    int10(0x0600,                        /* clear screen */
	  7*256,0,256*(rows-1)+columns-1); }
#endif

    /* fall thru */
  default:
    if (ch == 0x1B)
      esc_state = esc_esc;
    else if (ch == '\t') {
      int i;
      for (i = 8-cx%8; i--; ) handleansi(' '); }
    else if (ch == '\r') {
      cx = 0; goto updatecursor; }
    else if (ch == '\n') {
      handleansi('\r'); goto newline; }
    else if (ch == '\010') {
      if (cx) {
	cx--; goto updatecursor; } }
    else {
    doputchar:
#ifdef	GFX
      if (curmodetype == 'G' && (reverse ? fg : bg))
	int10(0x09DB,(attr/16)&0x07,1,0);/* set background in gfx mode */
#endif
#ifdef	GFX
      int10(0x0900|ch,                   /* output char */
	    curmodetype == 'H' ? reverse ? fg*256+bg : bg*256+fg :
	    256*scr+attr,1,0);
#else
      int10(0x0900|ch,256*scr+attr,1,0); /* output char */
#endif
      if (++cx == columns) {
	if (!wraparound)
	  cx--;
        else {
	  cx = 0;
	newline:
	  if (++cy == rows)
#ifdef	GFX
	    int10(0x0601,                /* scroll screen */
		  256*(curmodetype != 'T' ? reverse ? fg : bg : attr),0,
		  256*--cy+columns-1); }
#else
	    int10(0x0601,256*attr,0,     /* scroll screen */
		  256*--cy+columns-1); }
#endif
      }
    updatecursor:
      int10(0x0200,256*scr,0,256*cy+cx); /* update cursor position */ } }
  return;
}

#endif
