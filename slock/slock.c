/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

#include "arg.h"
#include "util.h"

char *argv0;

enum {
	INIT,
	INPUT,
	FAILED,
	NUMCOLS
};

struct lock {
	int screen;
	Window root, win;
	Pixmap pmap;
	unsigned long colors[NUMCOLS];
	unsigned long bgpixel;
	int w, h;
	GC gc;
	Pixmap bg, buf;	/* background image, and the frame drawn before it is copied to the window */
	XftDraw *xd;
	XftFont *ftime, *fdate, *finfo, *fpanic;
	XftColor timec, datec, failc, panicc;
	Visual *vis;
	Colormap cmap;
};

struct xrandr {
	int active;
	int evbase;
	int errbase;
};

#include "config.h"

static void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void)
{
	FILE *f;
	const char oomfile[] = "/proc/self/oom_score_adj";

	if (!(f = fopen(oomfile, "w"))) {
		if (errno == ENOENT)
			return;
		die("slock: fopen %s: %s\n", oomfile, strerror(errno));
	}
	fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
	if (fclose(f)) {
		if (errno == EACCES)
			die("slock: unable to disable OOM killer. "
			    "Make sure to suid or sgid slock.\n");
		else
			die("slock: fclose %s: %s\n", oomfile, strerror(errno));
	}
}
#endif

static const char *
gethash(void)
{
	const char *hash;
	struct passwd *pw;

	/* Check if the current user has a password entry */
	errno = 0;
	if (!(pw = getpwuid(getuid()))) {
		if (errno)
			die("slock: getpwuid: %s\n", strerror(errno));
		else
			die("slock: cannot retrieve password entry\n");
	}
	hash = pw->pw_passwd;

#if HAVE_SHADOW_H
	if (!strcmp(hash, "x")) {
		struct spwd *sp;
		if (!(sp = getspnam(pw->pw_name)))
			die("slock: getspnam: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = sp->sp_pwdp;
	}
#else
	if (!strcmp(hash, "*")) {
#ifdef __OpenBSD__
		if (!(pw = getpwuid_shadow(getuid())))
			die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = pw->pw_passwd;
#else
		die("slock: getpwuid: cannot retrieve shadow entry. "
		    "Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
	}
#endif /* HAVE_SHADOW_H */

	return hash;
}

/* background image, raw 8-bit PPM (P6) */
struct image {
	int w, h;
	unsigned char *rgb;
};

static struct image bgimg;

/* fake kernel panic, shown after panicafter seconds without a key */
#define PANICROWS 160
#define PANICCOLS 200
#define PANICTICK 45 /* ms between scroll steps */

static char panicrows[PANICROWS][PANICCOLS];
static int panicnext, panicstage, panictrace, panicoops = 1;
static long panicclock = 2871000000L; /* microseconds */
static int panicking;
static struct utsname panicuts;

static const char *panicfuncs[] = {
	"do_syscall_64", "entry_SYSCALL_64_after_hwframe", "__x64_sys_ioctl",
	"vfs_read", "ksys_read", "__schedule", "schedule", "do_idle",
	"cpu_startup_entry", "start_secondary", "ret_from_fork", "kthread",
	"worker_thread", "process_one_work", "nvme_irq", "drm_atomic_commit",
	"ext4_map_blocks", "jbd2_journal_commit_transaction", "tcp_v4_rcv",
	"ip_local_deliver", "__netif_receive_skb_core", "net_rx_action",
	"__do_softirq", "irq_exit_rcu", "common_interrupt", "mutex_lock",
	"kmem_cache_alloc", "page_fault_oops", "handle_mm_fault",
	"exc_page_fault", "asm_exc_page_fault", "filemap_read",
	"xhci_irq", "usb_hcd_irq", "rcu_core", "run_timer_softirq",
	"hrtimer_interrupt", "psi_task_change", "try_to_wake_up", "futex_wait",
};

static const char *panicreasons[] = {
	"Fatal exception in interrupt",
	"Attempted to kill init! exitcode=0x0000000b",
	"VFS: Unable to mount root fs on unknown-block(259,2)",
	"Out of memory and no killable processes...",
	"hung_task: blocked tasks",
	"corrupted stack end detected inside scheduler",
};

static const char *panicnames[] = {
	"swapper/0", "kworker/u16:3", "systemd", "Xorg", "dwm", "chromium",
	"ksoftirqd/1", "irq/131-nvme0q1",
};

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

static long long
nowms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static unsigned long
rnd(unsigned long n)
{
	return (unsigned long)rand() % n;
}

static void
panicadd(const char *fmt, ...)
{
	char b[PANICCOLS];
	va_list ap;
	int n;

	n = snprintf(b, sizeof(b), "[%5ld.%06ld] ", panicclock / 1000000, panicclock % 1000000);
	va_start(ap, fmt);
	vsnprintf(b + n, sizeof(b) - n, fmt, ap);
	va_end(ap);
	snprintf(panicrows[panicnext], PANICCOLS, "%s", b);
	panicnext = (panicnext + 1) % PANICROWS;
	panicclock += 100 + rnd(9000);
}

/* add one line to the fake kernel panic, in a cycle that restarts forever */
static void
panicstep(void)
{
	const char *fn = panicfuncs[rnd(LEN(panicfuncs))];
	const char *reason = panicreasons[panicoops % LEN(panicreasons)];

	switch (panicstage) {
	case 0:
		panicadd("BUG: unable to handle page fault for address: ffff%012lx", rnd(1UL << 40) * 16);
		break;
	case 1:
		panicadd("#PF: supervisor read access in kernel mode");
		break;
	case 2:
		panicadd("Oops: 0000 [#%d] PREEMPT SMP NOPTI", panicoops);
		break;
	case 3:
		panicadd("CPU: %lu PID: %lu Comm: %s Not tainted %s #1",
		         rnd(8), 1 + rnd(40000), panicnames[rnd(LEN(panicnames))], panicuts.release);
		break;
	case 4:
		panicadd("Hardware name: %s", panicuts.nodename);
		break;
	case 5:
		panicadd("Call Trace:");
		panictrace = 9 + rnd(8);
		break;
	case 6:
		panicadd(" <IRQ>");
		break;
	case 7:
		panicadd(" %s+0x%lx/0x%lx", fn, rnd(0x2f0), 0x100 + rnd(0x900));
		if (--panictrace > 0)
			return; /* stay in this stage for the whole trace */
		break;
	case 8:
		panicadd(" </IRQ>");
		break;
	case 9:
		panicadd("RIP: 0010:%s+0x%lx/0x%lx", fn, rnd(0x2f0), 0x100 + rnd(0x900));
		break;
	case 10:
		panicadd("RSP: 0018:ffffb%011lx EFLAGS: 00010246", rnd(1UL << 40));
		break;
	case 11:
		panicadd("RAX: %016lx RBX: %016lx RCX: %016lx", rnd(~0UL), rnd(~0UL), rnd(~0UL));
		break;
	case 12:
		panicadd("RDX: %016lx RSI: %016lx RDI: %016lx", rnd(~0UL), rnd(~0UL), rnd(~0UL));
		break;
	case 13:
		panicadd("Kernel panic - not syncing: %s", reason);
		break;
	case 14:
		panicadd("Kernel Offset: disabled");
		break;
	case 15:
		panicadd("---[ end Kernel panic - not syncing: %s ]---", reason);
		break;
	default:
		panicrows[panicnext][0] = '\0'; /* blank line between two oopses */
		panicnext = (panicnext + 1) % PANICROWS;
		panicoops++;
		panicstage = -1;
		break;
	}
	panicstage++;
}

static void
panicinit(void)
{
	int i;

	uname(&panicuts);
	srand((unsigned int)time(NULL));
	for (i = 0; i < PANICROWS; i++)
		panicrows[i][0] = '\0';
	panicnext = panicstage = panictrace = 0;
	panicoops = 1;
	panicclock = 2871000000L + rnd(1000000) * 1000;
	for (i = 0; i < 24; i++)
		panicstep();
}

static void
loadbg(void)
{
	char path[512];
	const char *home = getenv("HOME");
	FILE *f;
	int w, h, max;
	unsigned char *rgb;

	if (!home || !bgimage[0])
		return;
	snprintf(path, sizeof(path), "%s/%s", home, bgimage);
	if (!(f = fopen(path, "rb")))
		return;
	if (fscanf(f, "P6 %d %d %d", &w, &h, &max) != 3 || max != 255 ||
	    w < 1 || h < 1 || w > 16384 || h > 16384) {
		fclose(f);
		return;
	}
	fgetc(f); /* the single whitespace after the maximum value */
	if (!(rgb = malloc((size_t)w * h * 3))) {
		fclose(f);
		return;
	}
	if (fread(rgb, 3, (size_t)w * h, f) != (size_t)w * h) {
		free(rgb);
		fclose(f);
		return;
	}
	fclose(f);
	bgimg.w = w;
	bgimg.h = h;
	bgimg.rgb = rgb;
}

static int
maskshift(unsigned long mask)
{
	int s = 0;

	while (mask && !(mask & 1)) {
		mask >>= 1;
		s++;
	}
	return s;
}

static int
maskbits(unsigned long mask)
{
	int b = 0;

	for (; mask; mask >>= 1)
		b += mask & 1;
	return b;
}

/* (re)create the background and frame pixmaps for the current window size */
static void
setupbuffers(Display *dpy, struct lock *lock)
{
	int depth = DefaultDepth(dpy, lock->screen);
	int x, y, w, h, rs, gs, bs, rb, gb, bb;
	unsigned char *p;
	char *data;
	XImage *img;

	if (lock->xd)
		XftDrawDestroy(lock->xd);
	if (lock->bg)
		XFreePixmap(dpy, lock->bg);
	if (lock->buf)
		XFreePixmap(dpy, lock->buf);
	lock->bg = XCreatePixmap(dpy, lock->root, lock->w, lock->h, depth);
	lock->buf = XCreatePixmap(dpy, lock->root, lock->w, lock->h, depth);
	lock->xd = XftDrawCreate(dpy, lock->buf, lock->vis, lock->cmap);

	XSetForeground(dpy, lock->gc, lock->bgpixel);
	XFillRectangle(dpy, lock->bg, lock->gc, 0, 0, lock->w, lock->h);
	if (!bgimg.rgb)
		return;

	w = bgimg.w < lock->w ? bgimg.w : lock->w;
	h = bgimg.h < lock->h ? bgimg.h : lock->h;
	if (!(data = malloc((size_t)w * h * 4)))
		return;
	if (!(img = XCreateImage(dpy, lock->vis, depth, ZPixmap, 0, data, w, h, 32, 0))) {
		free(data);
		return;
	}
	rs = maskshift(lock->vis->red_mask);   rb = maskbits(lock->vis->red_mask);
	gs = maskshift(lock->vis->green_mask); gb = maskbits(lock->vis->green_mask);
	bs = maskshift(lock->vis->blue_mask);  bb = maskbits(lock->vis->blue_mask);
	for (y = 0; y < h; y++) {
		p = bgimg.rgb + (size_t)y * bgimg.w * 3;
		for (x = 0; x < w; x++, p += 3)
			XPutPixel(img, x, y, ((unsigned long)(p[0] >> (8 - rb)) << rs) |
			                      ((unsigned long)(p[1] >> (8 - gb)) << gs) |
			                      ((unsigned long)(p[2] >> (8 - bb)) << bs));
	}
	XPutImage(dpy, lock->bg, lock->gc, img, 0, 0, 0, 0, w, h);
	XDestroyImage(img); /* frees data too */
}

static void
drawcentered(Display *dpy, struct lock *lock, XftFont *font, XftColor *col,
             const char *str, int cx, int y)
{
	XGlyphInfo ext;

	XftTextExtentsUtf8(dpy, font, (const FcChar8 *)str, strlen(str), &ext);
	XftDrawStringUtf8(lock->xd, col, font, cx - ext.xOff / 2, y,
	                  (const FcChar8 *)str, strlen(str));
}

/* clock, date, state bar and failed attempts, centered on one monitor */
static void
drawmonitor(Display *dpy, struct lock *lock, int mx, int my, int mw, int mh,
            int color, unsigned int fails)
{
	char tbuf[64], dbuf[128], fbuf[64];
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	int cx = mx + mw / 2, top, y;
	int total = lock->ftime->height + 8 + lock->fdate->height + 28 +
	            barheight + 20 + lock->finfo->height;

	strftime(tbuf, sizeof(tbuf), timefmt, tm);
	strftime(dbuf, sizeof(dbuf), datefmt, tm);
	top = my + (mh - total) / 2;

	drawcentered(dpy, lock, lock->ftime, &lock->timec, tbuf, cx, top + lock->ftime->ascent);
	y = top + lock->ftime->height + 8;
	drawcentered(dpy, lock, lock->fdate, &lock->datec, dbuf, cx, y + lock->fdate->ascent);
	y += lock->fdate->height + 28;

	XSetForeground(dpy, lock->gc, lock->colors[color]);
	XFillRectangle(dpy, lock->buf, lock->gc, cx - barwidth / 2, y, barwidth, barheight);
	if (fails) {
		if (fails == 1)
			snprintf(fbuf, sizeof(fbuf), "1 failed attempt");
		else
			snprintf(fbuf, sizeof(fbuf), "%u failed attempts", fails);
		drawcentered(dpy, lock, lock->finfo, &lock->failc, fbuf, cx,
		             y + barheight + 20 + lock->finfo->ascent);
	}
}

static void
drawpanic(Display *dpy, struct lock *lock)
{
	int i, rows, row, y, fh = lock->fpanic->height;
	XftColor *col;

	XSetForeground(dpy, lock->gc, BlackPixel(dpy, lock->screen));
	XFillRectangle(dpy, lock->buf, lock->gc, 0, 0, lock->w, lock->h);
	rows = (lock->h - 8) / fh;
	if (rows > PANICROWS)
		rows = PANICROWS;
	for (i = 0; i < rows; i++) {
		row = (panicnext - 1 - i + PANICROWS * 2) % PANICROWS;
		if (!panicrows[row][0])
			continue;
		col = (strstr(panicrows[row], "Kernel panic") ||
		       strstr(panicrows[row], "---[ end")) ? &lock->failc : &lock->panicc;
		y = lock->h - 8 - i * fh - lock->fpanic->descent;
		XftDrawStringUtf8(lock->xd, col, lock->fpanic, 8, y,
		                  (const FcChar8 *)panicrows[row], strlen(panicrows[row]));
	}
}

static void
draw(Display *dpy, struct xrandr *rr, struct lock *lock, int color, unsigned int fails)
{
	XRRMonitorInfo *mons = NULL;
	int nmon = 0, i;

	if (panicking) {
		drawpanic(dpy, lock);
	} else {
		XCopyArea(dpy, lock->bg, lock->buf, lock->gc, 0, 0, lock->w, lock->h, 0, 0);
		if (rr->active)
			mons = XRRGetMonitors(dpy, lock->root, True, &nmon);
		if (mons && nmon > 0) {
			for (i = 0; i < nmon; i++)
				drawmonitor(dpy, lock, mons[i].x, mons[i].y,
				            mons[i].width, mons[i].height, color, fails);
		} else {
			drawmonitor(dpy, lock, 0, 0, lock->w, lock->h, color, fails);
		}
		if (mons)
			XRRFreeMonitors(mons);
	}
	XCopyArea(dpy, lock->buf, lock->win, lock->gc, 0, 0, lock->w, lock->h, 0, 0);
}

static void
redraw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       int color, unsigned int fails)
{
	int screen;

	for (screen = 0; screen < nscreens; screen++)
		draw(dpy, rr, locks[screen], color, fails);
}

static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       const char *hash)
{
	XRRScreenChangeNotifyEvent *rre;
	char buf[32], passwd[256], *inputhash;
	int num, screen, running, failure, oldc;
	unsigned int len, color, failcount, oldfails;
	long long lastkey, now;
	time_t lastclock, clocknow;
	KeySym ksym;
	XEvent ev;

	len = 0;
	running = 1;
	failure = 0;
	failcount = oldfails = 0;
	oldc = INIT;
	lastkey = nowms();
	lastclock = time(NULL);

	while (running) {
		/* timers: the clock, and the fake kernel panic after panicafter idle seconds */
		now = nowms();
		clocknow = time(NULL);
		if (panicking) {
			panicstep();
			redraw(dpy, rr, locks, nscreens, oldc, failcount);
		} else if (panicafter && now - lastkey >= (long long)panicafter * 1000) {
			panicking = 1;
			panicinit();
			redraw(dpy, rr, locks, nscreens, oldc, failcount);
		} else if (clocknow / 60 != lastclock / 60) {
			redraw(dpy, rr, locks, nscreens, oldc, failcount);
		}
		lastclock = clocknow;

		if (!XPending(dpy)) {
			struct pollfd pfd = { ConnectionNumber(dpy), POLLIN, 0 };

			poll(&pfd, 1, panicking ? PANICTICK : 1000);
			continue;
		}
		XNextEvent(dpy, &ev);

		if (ev.type == KeyPress) {
			lastkey = nowms();
			if (panicking) {
				/* any key ends the panic, and is not part of the password */
				panicking = 0;
				redraw(dpy, rr, locks, nscreens, oldc, failcount);
				continue;
			}
			explicit_bzero(&buf, sizeof(buf));
			num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
			if (IsKeypadKey(ksym)) {
				if (ksym == XK_KP_Enter)
					ksym = XK_Return;
				else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
					ksym = (ksym - XK_KP_0) + XK_0;
			}
			if (IsFunctionKey(ksym) ||
			    IsKeypadKey(ksym) ||
			    IsMiscFunctionKey(ksym) ||
			    IsPFKey(ksym) ||
			    IsPrivateKeypadKey(ksym))
				continue;
			switch (ksym) {
			case XK_Return:
				passwd[len] = '\0';
				errno = 0;
				if (!(inputhash = crypt(passwd, hash)))
					fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
				else
					running = !!strcmp(inputhash, hash);
				if (running) {
					XBell(dpy, 100);
					failure = 1;
					failcount++;
				}
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_Escape:
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_BackSpace:
				if (len)
					passwd[--len] = '\0';
				break;
			default:
				if (num && !iscntrl((int)buf[0]) &&
				    (len + num < sizeof(passwd))) {
					memcpy(passwd + len, buf, num);
					len += num;
				}
				break;
			}
			color = len ? INPUT : ((failure || failonclear) ? FAILED : INIT);
			if (running && (oldc != color || oldfails != failcount)) {
				redraw(dpy, rr, locks, nscreens, color, failcount);
				oldc = color;
				oldfails = failcount;
			}
		} else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
			rre = (XRRScreenChangeNotifyEvent*)&ev;
			for (screen = 0; screen < nscreens; screen++) {
				if (locks[screen]->win == rre->window) {
					if (rre->rotation == RR_Rotate_90 ||
					    rre->rotation == RR_Rotate_270) {
						locks[screen]->w = rre->height;
						locks[screen]->h = rre->width;
					} else {
						locks[screen]->w = rre->width;
						locks[screen]->h = rre->height;
					}
					XResizeWindow(dpy, locks[screen]->win,
					              locks[screen]->w, locks[screen]->h);
					setupbuffers(dpy, locks[screen]);
					draw(dpy, rr, locks[screen], oldc, failcount);
					break;
				}
			}
		} else if (ev.type == Expose) {
			if (ev.xexpose.count == 0)
				redraw(dpy, rr, locks, nscreens, oldc, failcount);
		} else {
			for (screen = 0; screen < nscreens; screen++)
				XRaiseWindow(dpy, locks[screen]->win);
		}
	}
}

static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i, ptgrab, kbgrab;
	struct lock *lock;
	XColor color, dummy;
	XSetWindowAttributes wa;
	Cursor invisible;

	if (dpy == NULL || screen < 0 || !(lock = malloc(sizeof(struct lock))))
		return NULL;

	lock->screen = screen;
	lock->root = RootWindow(dpy, lock->screen);

	for (i = 0; i < NUMCOLS; i++) {
		XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen),
		                 colorname[i], &color, &dummy);
		lock->colors[i] = color.pixel;
	}

	/* init */
	wa.override_redirect = 1;
	wa.background_pixel = lock->colors[INIT];
	lock->win = XCreateWindow(dpy, lock->root, 0, 0,
	                          DisplayWidth(dpy, lock->screen),
	                          DisplayHeight(dpy, lock->screen),
	                          0, DefaultDepth(dpy, lock->screen),
	                          CopyFromParent,
	                          DefaultVisual(dpy, lock->screen),
	                          CWOverrideRedirect | CWBackPixel, &wa);
	lock->w = DisplayWidth(dpy, lock->screen);
	lock->h = DisplayHeight(dpy, lock->screen);
	lock->vis = DefaultVisual(dpy, lock->screen);
	lock->cmap = DefaultColormap(dpy, lock->screen);
	lock->gc = XCreateGC(dpy, lock->root, 0, NULL);
	lock->bg = lock->buf = 0;
	lock->xd = NULL;
	lock->ftime = XftFontOpenName(dpy, lock->screen, timefont);
	lock->fdate = XftFontOpenName(dpy, lock->screen, datefont);
	lock->finfo = XftFontOpenName(dpy, lock->screen, infofont);
	lock->fpanic = XftFontOpenName(dpy, lock->screen, panicfont);
	if (!lock->ftime || !lock->fdate || !lock->finfo || !lock->fpanic)
		die("slock: cannot load fonts\n");
	XftColorAllocName(dpy, lock->vis, lock->cmap, timecolor, &lock->timec);
	XftColorAllocName(dpy, lock->vis, lock->cmap, datecolor, &lock->datec);
	XftColorAllocName(dpy, lock->vis, lock->cmap, colorname[FAILED], &lock->failc);
	XftColorAllocName(dpy, lock->vis, lock->cmap, panictextcolor, &lock->panicc);
	XAllocNamedColor(dpy, lock->cmap, bgcolor, &color, &dummy);
	lock->bgpixel = color.pixel;
	setupbuffers(dpy, lock);
	lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap,
	                                &color, &color, 0, 0);
	XDefineCursor(dpy, lock->win, invisible);

	/* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
	for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
		if (ptgrab != GrabSuccess) {
			ptgrab = XGrabPointer(dpy, lock->root, False,
			                      ButtonPressMask | ButtonReleaseMask |
			                      PointerMotionMask, GrabModeAsync,
			                      GrabModeAsync, None, invisible, CurrentTime);
		}
		if (kbgrab != GrabSuccess) {
			kbgrab = XGrabKeyboard(dpy, lock->root, True,
			                       GrabModeAsync, GrabModeAsync, CurrentTime);
		}

		/* input is grabbed: we can lock the screen */
		if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
			XSelectInput(dpy, lock->win, ExposureMask);
			XMapRaised(dpy, lock->win);
			if (rr->active)
				XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);
			draw(dpy, rr, lock, INIT, 0);

			XSelectInput(dpy, lock->root, SubstructureNotifyMask);
			return lock;
		}

		/* retry on AlreadyGrabbed but fail on other errors */
		if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
		    (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
			break;

		usleep(100000);
	}

	/* we couldn't grab all input: fail out */
	if (ptgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
		        screen);
	if (kbgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
		        screen);
	return NULL;
}

static void
usage(void)
{
	die("usage: slock [-v] [cmd [arg ...]]\n");
}

int
main(int argc, char **argv) {
	struct xrandr rr;
	struct lock **locks;
	struct passwd *pwd;
	struct group *grp;
	uid_t duid;
	gid_t dgid;
	const char *hash;
	Display *dpy;
	int s, nlocks, nscreens;

	ARGBEGIN {
	case 'v':
		puts("slock-"VERSION);
		return 0;
	default:
		usage();
	} ARGEND

	/* validate drop-user and -group */
	errno = 0;
	if (!(pwd = getpwnam(user)))
		die("slock: getpwnam %s: %s\n", user,
		    errno ? strerror(errno) : "user entry not found");
	duid = pwd->pw_uid;
	errno = 0;
	if (!(grp = getgrnam(group)))
		die("slock: getgrnam %s: %s\n", group,
		    errno ? strerror(errno) : "group entry not found");
	dgid = grp->gr_gid;

#ifdef __linux__
	dontkillme();
#endif

	hash = gethash();
	errno = 0;
	if (!crypt("", hash))
		die("slock: crypt: %s\n", strerror(errno));

	if (!(dpy = XOpenDisplay(NULL)))
		die("slock: cannot open display\n");

	/* drop privileges */
	if (setgroups(0, NULL) < 0)
		die("slock: setgroups: %s\n", strerror(errno));
	if (setgid(dgid) < 0)
		die("slock: setgid: %s\n", strerror(errno));
	if (setuid(duid) < 0)
		die("slock: setuid: %s\n", strerror(errno));

	loadbg();

	/* check for Xrandr support */
	rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

	/* get number of screens in display "dpy" and blank them */
	nscreens = ScreenCount(dpy);
	if (!(locks = calloc(nscreens, sizeof(struct lock *))))
		die("slock: out of memory\n");
	for (nlocks = 0, s = 0; s < nscreens; s++) {
		if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
			nlocks++;
		else
			break;
	}
	XSync(dpy, 0);

	/* did we manage to lock everything? */
	if (nlocks != nscreens)
		return 1;

	/* run post-lock command */
	if (argc > 0) {
		switch (fork()) {
		case -1:
			die("slock: fork failed: %s\n", strerror(errno));
		case 0:
			if (close(ConnectionNumber(dpy)) < 0)
				die("slock: close: %s\n", strerror(errno));
			execvp(argv[0], argv);
			fprintf(stderr, "slock: execvp %s: %s\n", argv[0], strerror(errno));
			_exit(1);
		}
	}

	/* everything is now blank. Wait for the correct password */
	readpw(dpy, &rr, locks, nscreens, hash);

	return 0;
}
