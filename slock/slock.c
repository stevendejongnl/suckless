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

/* colors of the fake panic text, see panicpalette in config.h */
enum {
	PC_NORM,
	PC_WARN,
	PC_ERR,
	PC_OK,
	PC_INFO,
	PC_DIM,
	PC_NUM
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
	XftFont *fbsodbig, *fbsod, *fbsodsmall;
	XftColor timec, datec, failc, whitec;
	XftColor pc[PC_NUM];
	unsigned long bsodpixel;
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

/* fake panic, shown after panicafter seconds without a key: several kinds of
 * Linux failures, one after the other, or a Windows bluescreen */
#define PANICROWS 160
#define PANICCOLS 200
#define PANICTICK 45 /* ms between scroll steps */
#define NSCEN 7

struct panicrow {
	char ts[40], tag[12], msg[PANICCOLS];
	unsigned char tagcls, msgcls;
};

static struct panicrow panicrows[PANICROWS];
static int panicnext, panicscen, panicstage, panicstay, panicdone, panictrace;
static int panicoops = 1, panicpause, panicpid;
static const char *panicproc = "";
static long panicclock;
static int panicking, panicbsod, bsodpct, bsodstop;
static unsigned int bsodseed;
static long long bsodnext;
static struct utsname panicuts;

static const char *panicfuncs[] = {
	"do_syscall_64", "entry_SYSCALL_64_after_hwframe", "__x64_sys_ioctl",
	"vfs_read", "ksys_read", "__schedule", "schedule", "do_idle",
	"cpu_startup_entry", "start_secondary", "ret_from_fork", "kthread",
	"worker_thread", "process_one_work", "nvme_irq", "drm_atomic_commit",
	"ext4_map_blocks", "jbd2_journal_commit_transaction", "tcp_v4_rcv",
	"ip_local_deliver", "__netif_receive_skb_core", "net_rx_action",
	"__do_softirq", "irq_exit_rcu", "common_interrupt", "mutex_lock",
	"kmem_cache_alloc", "handle_mm_fault", "exc_page_fault",
	"asm_exc_page_fault", "filemap_read", "xhci_irq", "usb_hcd_irq",
	"rcu_core", "run_timer_softirq", "hrtimer_interrupt", "psi_task_change",
	"try_to_wake_up", "futex_wait", "i915_gem_object_unbind", "dma_fence_wait_timeout",
};

static const char *panicnames[] = {
	"swapper/0", "kworker/u16:3", "systemd", "Xorg", "dwm", "chromium",
	"ksoftirqd/1", "irq/131-nvme0q1", "docker", "python3", "slack", "code",
};

static const char *panicmods[] = {
	"nvidia_drm(POE)", "nvidia_modeset(POE)", "nvidia(POE)", "snd_hda_intel",
	"iwlmvm", "btusb", "i915", "kvm_intel", "kvm", "xhci_pci", "nvme",
	"thunderbolt", "uvcvideo", "vboxdrv(OE)", "snd_usb_audio",
};

static const char *panicsvcs[] = {
	"Network Manager", "Docker Application Container Engine", "OpenSSH Daemon",
	"D-Bus System Message Bus", "User Login Management", "Bluetooth service",
	"Authorization Manager", "Hostname Service", "Time & Date Service",
	"CUPS Scheduler", "Avahi mDNS/DNS-SD Stack", "PipeWire Multimedia Service",
	"Display Manager", "Firewall Daemon", "Journal Service",
};

static const char *panicunits[] = {
	"NetworkManager", "docker", "sshd", "dbus", "systemd-logind", "bluetooth",
	"polkit", "systemd-hostnamed", "systemd-timedated", "cups", "avahi-daemon",
	"pipewire", "display-manager", "firewalld", "systemd-journald",
};

static const char *panictargets[] = {
	"Basic System", "Network", "Multi-User System", "Graphical Interface",
	"Local File Systems", "Sockets", "Timer Units", "Sound Card",
};

static const char *panicmounts[] = { "/boot", "/home", "/var/lib/docker", "/tmp", "/var/log" };

static const char *bsodstops[] = {
	"CRITICAL_PROCESS_DIED", "SYSTEM_SERVICE_EXCEPTION", "IRQL_NOT_LESS_OR_EQUAL",
	"PAGE_FAULT_IN_NONPAGED_AREA", "KERNEL_SECURITY_CHECK_FAILURE",
	"DPC_WATCHDOG_VIOLATION", "MEMORY_MANAGEMENT", "UNEXPECTED_STORE_EXCEPTION",
	"INACCESSIBLE_BOOT_DEVICE", "VIDEO_TDR_FAILURE", "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED",
};

static const char *bsodfails[] = {
	"nvlddmkm.sys", "ntoskrnl.exe", "win32kfull.sys", "ntfs.sys", "tcpip.sys",
	"dxgkrnl.sys", "Wdf01000.sys", "storport.sys",
};

#define LEN(a) (sizeof(a) / sizeof((a)[0]))
#define PICK(a) ((a)[rnd(LEN(a))])

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

/* a random value over the whole 64 bits */
static unsigned long
rnd64(void)
{
	return ((unsigned long)rand() << 33) ^ ((unsigned long)rand() << 11) ^ (unsigned long)rand();
}

/* add a row: optional kernel timestamp, optional colored tag, then the message */
static void
padd(int stamp, const char *tag, int tagcls, int msgcls, const char *fmt, ...)
{
	struct panicrow *r = &panicrows[panicnext];
	va_list ap;

	r->ts[0] = '\0';
	if (stamp)
		snprintf(r->ts, sizeof(r->ts), "[%5ld.%06ld] ", panicclock / 1000000, panicclock % 1000000);
	snprintf(r->tag, sizeof(r->tag), "%s", tag ? tag : "");
	r->tagcls = tagcls;
	r->msgcls = msgcls;
	va_start(ap, fmt);
	vsnprintf(r->msg, sizeof(r->msg), fmt, ap);
	va_end(ap);
	panicnext = (panicnext + 1) % PANICROWS;
	panicclock += 100 + rnd(9000);
}

#define LOG(cls, ...) padd(1, NULL, 0, cls, __VA_ARGS__)

static void
panicblank(void)
{
	padd(0, NULL, 0, PC_NORM, "%s", "");
}

/* one call trace entry, repeated until panictrace runs out */
static void
tracerow(void)
{
	LOG(PC_NORM, " %s+0x%lx/0x%lx", PICK(panicfuncs), rnd(0x2f0), 0x100 + rnd(0x900));
	if (--panictrace > 0)
		panicstay = 1;
}

static void
cpuline(void)
{
	LOG(PC_NORM, "CPU: %lu PID: %lu Comm: %s Not tainted %s #1 PREEMPT_DYNAMIC",
	    rnd(8), 1 + rnd(40000), PICK(panicnames), panicuts.release);
}

static void
regrows(int stage)
{
	if (stage == 0)
		LOG(PC_DIM, "RAX: %016lx RBX: %016lx RCX: %016lx", rnd64(), rnd64(), rnd64());
	else
		LOG(PC_DIM, "RDX: %016lx RSI: %016lx RDI: %016lx", rnd64(), rnd64(), rnd64());
}

/* each scenario adds one row per call, and sets panicdone after its last row */
static void
sc_oops(void)
{
	switch (panicstage) {
	case 0: LOG(PC_ERR, "BUG: unable to handle page fault for address: ffff%012lx", rnd64() & 0xffffffffffffUL); break;
	case 1: LOG(PC_WARN, "#PF: supervisor read access in kernel mode"); break;
	case 2: LOG(PC_ERR, "Oops: 0000 [#%d] PREEMPT SMP NOPTI", panicoops); break;
	case 3: cpuline(); break;
	case 4: LOG(PC_DIM, "Hardware name: %s", panicuts.nodename); break;
	case 5: LOG(PC_INFO, "Call Trace:"); panictrace = 9 + rnd(8); break;
	case 6: LOG(PC_DIM, " <IRQ>"); break;
	case 7: tracerow(); break;
	case 8: LOG(PC_DIM, " </IRQ>"); break;
	case 9: LOG(PC_NORM, "RIP: 0010:%s+0x%lx/0x%lx", PICK(panicfuncs), rnd(0x2f0), 0x100 + rnd(0x900)); break;
	case 10: LOG(PC_DIM, "RSP: 0018:ffffb%011lx EFLAGS: 00010246", rnd64() & 0xffffffffffUL); break;
	case 11: regrows(0); break;
	case 12: regrows(1); break;
	case 13: LOG(PC_ERR, "Kernel panic - not syncing: Fatal exception in interrupt"); break;
	case 14: LOG(PC_DIM, "Kernel Offset: disabled"); break;
	default: LOG(PC_ERR, "---[ end Kernel panic - not syncing: Fatal exception in interrupt ]---"); panicdone = 1; break;
	}
}

static void
sc_nullptr(void)
{
	int i;
	char code[64] = "";

	switch (panicstage) {
	case 0: LOG(PC_ERR, "BUG: kernel NULL pointer dereference, address: 0000000000000%03lx", rnd(0x200)); break;
	case 1: LOG(PC_WARN, "#PF: supervisor write access in kernel mode"); break;
	case 2: LOG(PC_WARN, "#PF: error_code(0x0002) - not-present page"); break;
	case 3: LOG(PC_DIM, "PGD 0 P4D 0"); break;
	case 4: LOG(PC_ERR, "Oops: 0002 [#%d] PREEMPT SMP NOPTI", panicoops); break;
	case 5: cpuline(); break;
	case 6: LOG(PC_NORM, "RIP: 0010:%s+0x%lx/0x%lx", PICK(panicfuncs), rnd(0x2f0), 0x100 + rnd(0x900)); break;
	case 7:
		for (i = 0; i < 12; i++)
			snprintf(code + i * 3, sizeof(code) - i * 3, "%02lx ", rnd(256));
		LOG(PC_DIM, "Code: %s", code);
		break;
	case 8: LOG(PC_DIM, "RSP: 0018:ffffb%011lx EFLAGS: 00010286", rnd64() & 0xffffffffffUL); break;
	case 9: regrows(0); break;
	case 10: LOG(PC_INFO, "Call Trace:"); panictrace = 6 + rnd(8); break;
	case 11: tracerow(); break;
	case 12: LOG(PC_DIM, "CR2: 0000000000000%03lx CR3: 00000001%08lx CR4: 00000000003506f0", rnd(0x200), rnd64() & 0xffffffffUL); break;
	case 13: LOG(PC_ERR, "Kernel panic - not syncing: Fatal exception"); break;
	default: LOG(PC_ERR, "---[ end Kernel panic - not syncing: Fatal exception ]---"); panicdone = 1; break;
	}
}

static void
sc_lockup(void)
{
	int cpu = (int)rnd(8);

	switch (panicstage) {
	case 0: LOG(PC_ERR, "watchdog: BUG: soft lockup - CPU#%d stuck for %lus! [%s:%lu]", cpu, 22 + rnd(40), PICK(panicnames), 1 + rnd(30000)); break;
	case 1: LOG(PC_DIM, "Modules linked in: %s %s %s %s", PICK(panicmods), PICK(panicmods), PICK(panicmods), PICK(panicmods)); break;
	case 2: cpuline(); break;
	case 3: LOG(PC_NORM, "RIP: 0010:%s+0x%lx/0x%lx", PICK(panicfuncs), rnd(0x2f0), 0x100 + rnd(0x900)); break;
	case 4: LOG(PC_INFO, "Call Trace:"); panictrace = 5 + rnd(6); break;
	case 5: tracerow(); break;
	case 6: LOG(PC_WARN, "rcu: INFO: rcu_preempt self-detected stall on CPU"); break;
	case 7: LOG(PC_DIM, "rcu:     %d-....: (%lu ticks this GP) idle=%03lx/1/0x4000000000000000 softirq=%lu/%lu fqs=%lu", cpu, 20000 + rnd(9000), rnd(4096), rnd(900000), rnd(900000), rnd(9000)); break;
	case 8: LOG(PC_DIM, "rcu:     (t=%lu jiffies g=%lu q=%lu ncpus=8)", 60000 + rnd(9000), rnd(90000), rnd(9000)); break;
	case 9: LOG(PC_ERR, "Kernel panic - not syncing: softlockup: hung tasks"); break;
	default: LOG(PC_ERR, "---[ end Kernel panic - not syncing: softlockup: hung tasks ]---"); panicdone = 1; break;
	}
}

static void
sc_oom(void)
{
	switch (panicstage) {
	case 0:
		panicproc = PICK(panicnames);
		panicpid = 1000 + (int)rnd(40000);
		LOG(PC_WARN, "%s invoked oom-killer: gfp_mask=0x140cca(GFP_HIGHUSER_MOVABLE|__GFP_COMP), order=0, oom_score_adj=0", panicproc);
		break;
	case 1: cpuline(); break;
	case 2: LOG(PC_INFO, "Call Trace:"); panictrace = 5 + rnd(4); break;
	case 3: tracerow(); break;
	case 4: LOG(PC_INFO, "Mem-Info:"); break;
	case 5: LOG(PC_DIM, "active_anon:%lu inactive_anon:%lu isolated_anon:0", 2000000 + rnd(900000), 900000 + rnd(500000)); break;
	case 6: LOG(PC_DIM, " active_file:%lu inactive_file:%lu isolated_file:0", rnd(900), rnd(1300)); break;
	case 7: LOG(PC_NORM, "Tasks state (memory values in pages):"); panictrace = 6 + rnd(4); break;
	case 8: LOG(PC_DIM, "[  pid  ]   uid  tgid total_vm      rss pgtables_bytes swapents oom_score_adj name"); break;
	case 9:
		LOG(PC_NORM, "[%7lu]  %5d %5lu %8lu %8lu %8lu %8d %5d %s", 100 + rnd(40000), 1000, rnd(40000),
		    rnd(4000000), rnd(2500000), rnd(40000000), 0, 0, PICK(panicnames));
		if (--panictrace > 0)
			panicstay = 1;
		break;
	case 10: LOG(PC_DIM, "oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),cpuset=/,mems_allowed=0,global_oom,task_memcg=/user.slice,task=%s,pid=%d,uid=1000", panicproc, panicpid); break;
	case 11: LOG(PC_ERR, "Out of memory: Killed process %d (%s) total-vm:%lukB, anon-rss:%lukB, file-rss:%lukB, shmem-rss:0kB, UID:1000 pgtables:%lukB oom_score_adj:0", panicpid, panicproc, 4000000 + rnd(8000000), 2000000 + rnd(6000000), rnd(90000), 20000 + rnd(30000)); break;
	default: LOG(PC_OK, "oom_reaper: reaped process %d (%s), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB", panicpid, panicproc); panicdone = 1; break;
	}
}

static void
sc_disk(void)
{
	switch (panicstage) {
	case 0: LOG(PC_WARN, "nvme nvme0: I/O %lu QID %lu timeout, aborting", rnd(900), 1 + rnd(8)); break;
	case 1: LOG(PC_DIM, "nvme nvme0: Abort status: 0x0"); break;
	case 2: LOG(PC_WARN, "nvme nvme0: I/O %lu QID %lu timeout, reset controller", rnd(900), 1 + rnd(8)); break;
	case 3: LOG(PC_ERR, "nvme nvme0: controller is down; will reset: CSTS=0xffffffff, PCI_STATUS=0x10"); break;
	case 4: LOG(PC_ERR, "nvme nvme0: Disabling device after reset failure: -19"); panictrace = 4 + rnd(4); break;
	case 5:
		LOG(PC_ERR, "blk_update_request: I/O error, dev nvme0n1, sector %lu op 0x%d:(%s) flags 0x%lx phys_seg 1 prio class 2",
		    rnd(900000000), (int)(rnd(2)), rnd(2) ? "READ" : "WRITE", 0x80700 + rnd(0x100));
		if (--panictrace > 0)
			panicstay = 1;
		else
			panictrace = 3;
		break;
	case 6:
		LOG(PC_WARN, "Buffer I/O error on dev nvme0n1p2, logical block %lu, lost async page write", rnd(90000000));
		if (--panictrace > 0)
			panicstay = 1;
		break;
	case 7: LOG(PC_ERR, "EXT4-fs error (device nvme0n1p2): ext4_find_entry:1455: inode #%lu: comm %s: reading directory lblock 0", 100000 + rnd(900000), PICK(panicnames)); break;
	case 8: LOG(PC_WARN, "EXT4-fs (nvme0n1p2): Remounting filesystem read-only"); break;
	case 9: LOG(PC_WARN, "Aborting journal on device nvme0n1p2-8."); break;
	case 10: LOG(PC_ERR, "JBD2: I/O error when updating journal superblock for nvme0n1p2-8."); break;
	case 11: LOG(PC_ERR, "Kernel panic - not syncing: EXT4-fs (device nvme0n1p2): panic forced after error"); break;
	default: LOG(PC_ERR, "---[ end Kernel panic - not syncing: EXT4-fs (device nvme0n1p2): panic forced after error ]---"); panicdone = 1; break;
	}
}

static void
sc_systemd(void)
{
	int k = (int)rnd(LEN(panicsvcs));
	unsigned long r = rnd(100);

	switch (panicstage) {
	case 0:
		panictrace = 28 + rnd(20);
		panicpid = 0;
		break;
	case 1:
		if (panicpid) { /* the line that follows a failed unit */
			padd(0, NULL, 0, PC_DIM, "See 'systemctl status %s.service' for details.", panicunits[panicpid - 1]);
			panicpid = 0;
		} else if (r < 70) {
			padd(0, "[  OK  ] ", PC_OK, PC_NORM, "Started %s.", panicsvcs[k]);
		} else if (r < 80) {
			padd(0, "[  OK  ] ", PC_OK, PC_NORM, "Reached target %s.", PICK(panictargets));
		} else if (r < 88) {
			padd(0, "[  OK  ] ", PC_OK, PC_NORM, "Mounted %s.", PICK(panicmounts));
		} else if (r < 94) {
			padd(0, "[FAILED] ", PC_ERR, PC_NORM, "Failed to start %s.", panicsvcs[k]);
			panicpid = k + 1; /* remembered for the "See systemctl status" line */
		} else if (r < 98) {
			padd(0, "[ TIME ] ", PC_WARN, PC_NORM, "Timed out waiting for device /dev/disk/by-uuid/%08lx-%04lx-%04lx.", rnd64() & 0xffffffffUL, rnd(65536), rnd(65536));
		} else {
			padd(0, "[DEPEND] ", PC_WARN, PC_NORM, "Dependency failed for %s.", PICK(panictargets));
		}
		if (--panictrace > 0) {
			panicstay = 1;
			return;
		}
		panicstage = 1; /* continue with the emergency mode text */
		break;
	case 2: padd(0, NULL, 0, PC_ERR, "%s", "Welcome to emergency mode! After logging in, type \"journalctl -xb\" to view"); break;
	case 3: padd(0, NULL, 0, PC_ERR, "%s", "system logs, \"systemctl reboot\" to reboot, or \"exit\" to continue booting."); break;
	case 4: padd(0, NULL, 0, PC_WARN, "%s", "Cannot open access to console, the root account is locked."); break;
	case 5: padd(0, NULL, 0, PC_DIM, "%s", "See sulogin(8) man page for more details."); break;
	default: padd(0, NULL, 0, PC_NORM, "%s", "Press Enter to continue."); panicdone = 1; break;
	}
}

static void
sc_mce(void)
{
	switch (panicstage) {
	case 0: LOG(PC_WARN, "mce: [Hardware Error]: Machine check events logged"); break;
	case 1: LOG(PC_ERR, "mce: [Hardware Error]: CPU %lu: Machine Check: 0 Bank %lu: be00000000800400", rnd(8), rnd(24)); break;
	case 2: LOG(PC_ERR, "mce: [Hardware Error]: TSC 0 ADDR %lx MISC %lx", rnd64() & 0xfffffffffUL, rnd64() & 0xffffUL); break;
	case 3: LOG(PC_DIM, "mce: [Hardware Error]: PROCESSOR 0:a0652 TIME %lu SOCKET 0 APIC %lu microcode f6", 1700000000 + rnd(90000000), rnd(16)); break;
	case 4: LOG(PC_ERR, "Kernel panic - not syncing: Fatal machine check"); break;
	case 5: LOG(PC_WARN, "Shutting down cpus with NMI"); break;
	case 6: LOG(PC_DIM, "Kernel Offset: disabled"); break;
	default: LOG(PC_ERR, "---[ end Kernel panic - not syncing: Fatal machine check ]---"); panicdone = 1; break;
	}
}

/* pick the next Linux scenario, never the same one twice in a row */
static void
panicpick(void)
{
	panicscen = (panicscen + 1 + (int)rnd(NSCEN - 1)) % NSCEN;
	panicstage = 0;
	panicpid = 0;
}

/* add one row to the fake panic */
static void
panicline(void)
{
	panicstay = 0;
	switch (panicscen) {
	case 0: sc_oops(); break;
	case 1: sc_nullptr(); break;
	case 2: sc_lockup(); break;
	case 3: sc_oom(); break;
	case 4: sc_disk(); break;
	case 5: sc_systemd(); break;
	default: sc_mce(); break;
	}
	if (panicdone) {
		panicdone = 0;
		panicblank();
		panicoops++;
		panicpick();
	} else if (!panicstay) {
		panicstage++;
	}
}

/* the bluescreen counts up slowly, waits at 100% and starts over with a new stop code */
static int
bsodstep(long long t)
{
	if (t < bsodnext)
		return 0;
	if (bsodpct >= 100) {
		bsodpct = 0;
		bsodstop = (int)rnd(LEN(bsodstops));
		bsodseed = (unsigned int)rand();
		bsodnext = t + 1500;
		return 1;
	}
	bsodpct += 1 + (rnd(6) == 0 ? (int)rnd(4) : 0);
	if (bsodpct > 100)
		bsodpct = 100;
	bsodnext = t + (bsodpct >= 100 ? 2500 : 350 + rnd(1300));
	return 1;
}

/* advance the fake panic by one tick, returns 1 when the screen has to be redrawn */
static int
panicstep(void)
{
	int n, changed = 0;

	if (panicbsod)
		return bsodstep(nowms());
	if (panicpause > 0) {
		panicpause--;
		return 0;
	}
	n = 1 + (int)rnd(4);
	if (rnd(35) == 0)
		panicpause = 8 + (int)rnd(25); /* the "machine" stalls for a moment */
	while (n-- > 0) {
		panicline();
		changed = 1;
	}
	return changed;
}

static void
panicinit(void)
{
	int i;

	uname(&panicuts);
	srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
	for (i = 0; i < PANICROWS; i++) {
		panicrows[i].ts[0] = panicrows[i].tag[0] = panicrows[i].msg[0] = '\0';
	}
	panicnext = panicstay = panicdone = panicpause = 0;
	panicoops = 1;
	panicclock = 2871000000L + (long)rnd(1000000) * 1000;
	panicbsod = rnd(8) == 0; /* one start in eight is a Windows bluescreen */
	if (panicbsod) {
		bsodpct = 0;
		bsodstop = (int)rnd(LEN(bsodstops));
		bsodseed = (unsigned int)rand();
		bsodnext = nowms() + 1200;
		return;
	}
	panicscen = (int)rnd(NSCEN);
	panicstage = 0;
	panicpid = 0;
	for (i = 0; i < 24; i++)
		panicline();
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

/* draw a string, returns the x where the next one starts */
static int
pseg(Display *dpy, struct lock *lock, int cls, int x, int y, const char *str)
{
	XGlyphInfo ext;

	if (!*str)
		return x;
	XftDrawStringUtf8(lock->xd, &lock->pc[cls], lock->fpanic, x, y,
	                  (const FcChar8 *)str, strlen(str));
	XftTextExtentsUtf8(dpy, lock->fpanic, (const FcChar8 *)str, strlen(str), &ext);
	return x + ext.xOff;
}

static void
drawpanic(Display *dpy, struct lock *lock)
{
	int i, rows, row, x, y, fh = lock->fpanic->height;
	struct panicrow *r;

	XSetForeground(dpy, lock->gc, BlackPixel(dpy, lock->screen));
	XFillRectangle(dpy, lock->buf, lock->gc, 0, 0, lock->w, lock->h);
	rows = (lock->h - 8) / fh;
	if (rows > PANICROWS)
		rows = PANICROWS;
	for (i = 0; i < rows; i++) {
		row = (panicnext - 1 - i + PANICROWS * 2) % PANICROWS;
		r = &panicrows[row];
		if (!r->ts[0] && !r->tag[0] && !r->msg[0])
			continue;
		y = lock->h - 8 - i * fh - lock->fpanic->descent;
		x = pseg(dpy, lock, PC_DIM, 8, y, r->ts);
		x = pseg(dpy, lock, r->tagcls, x, y, r->tag);
		pseg(dpy, lock, r->msgcls, x, y, r->msg);
	}
}

static void
bsodtext(Display *dpy, struct lock *lock, XftFont *font, int x, int y, const char *str)
{
	XftDrawStringUtf8(lock->xd, &lock->whitec, font, x, y,
	                  (const FcChar8 *)str, strlen(str));
}

/* the fake QR code: three finder patterns and random modules from the seed */
static void
bsodqr(Display *dpy, struct lock *lock, int x, int y, int mod)
{
	enum { N = 29 };
	unsigned int st = bsodseed ? bsodseed : 1;
	int cx, cy, f;
	unsigned long white = WhitePixel(dpy, lock->screen), black = BlackPixel(dpy, lock->screen);
	static const int fx[3] = { 0, N - 7, 0 }, fy[3] = { 0, 0, N - 7 };

	XSetForeground(dpy, lock->gc, white);
	XFillRectangle(dpy, lock->buf, lock->gc, x - mod * 2, y - mod * 2, (N + 4) * mod, (N + 4) * mod);
	XSetForeground(dpy, lock->gc, black);
	for (cy = 0; cy < N; cy++) {
		for (cx = 0; cx < N; cx++) {
			st = st * 1664525u + 1013904223u;
			for (f = 0; f < 3; f++)
				if (cx >= fx[f] - 1 && cx <= fx[f] + 7 && cy >= fy[f] - 1 && cy <= fy[f] + 7)
					break;
			if (f < 3 || !((st >> 16) & 1))
				continue;
			XFillRectangle(dpy, lock->buf, lock->gc, x + cx * mod, y + cy * mod, mod, mod);
		}
	}
	for (f = 0; f < 3; f++) {
		XSetForeground(dpy, lock->gc, black);
		XFillRectangle(dpy, lock->buf, lock->gc, x + fx[f] * mod, y + fy[f] * mod, 7 * mod, 7 * mod);
		XSetForeground(dpy, lock->gc, white);
		XFillRectangle(dpy, lock->buf, lock->gc, x + (fx[f] + 1) * mod, y + (fy[f] + 1) * mod, 5 * mod, 5 * mod);
		XSetForeground(dpy, lock->gc, black);
		XFillRectangle(dpy, lock->buf, lock->gc, x + (fx[f] + 2) * mod, y + (fy[f] + 2) * mod, 3 * mod, 3 * mod);
	}
}

/* the Windows bluescreen on one monitor */
static void
drawbsodmon(Display *dpy, struct lock *lock, int mx, int my, int mw, int mh)
{
	char b[160];
	int x = mx + mw * 12 / 100, y = my + mh * 12 / 100;
	int mod = mh >= 1200 ? 5 : 4;
	int qry, tx;

	y += lock->fbsodbig->ascent;
	bsodtext(dpy, lock, lock->fbsodbig, x, y, ":(");
	y += lock->fbsodbig->descent + lock->fbsod->height;
	bsodtext(dpy, lock, lock->fbsod, x, y, "Your PC ran into a problem and needs to restart. We're just");
	y += lock->fbsod->height + 4;
	bsodtext(dpy, lock, lock->fbsod, x, y, "collecting some error info, and then we'll restart for you.");
	y += lock->fbsod->height * 2;
	snprintf(b, sizeof(b), "%d%% complete", bsodpct);
	bsodtext(dpy, lock, lock->fbsod, x, y, b);

	qry = y + lock->fbsod->height + mod * 2;
	bsodqr(dpy, lock, x + mod * 2, qry + mod * 2, mod);
	tx = x + (29 + 6) * mod;
	y = qry + mod * 2 + lock->fbsodsmall->ascent;
	bsodtext(dpy, lock, lock->fbsodsmall, tx, y, "For more information about this issue and possible fixes, visit");
	y += lock->fbsodsmall->height;
	bsodtext(dpy, lock, lock->fbsodsmall, tx, y, "https://www.windows.com/stopcode");
	y += lock->fbsodsmall->height * 2;
	bsodtext(dpy, lock, lock->fbsodsmall, tx, y, "If you call a support person, give them this info:");
	y += lock->fbsodsmall->height;
	snprintf(b, sizeof(b), "Stop code: %s", bsodstops[bsodstop % LEN(bsodstops)]);
	bsodtext(dpy, lock, lock->fbsodsmall, tx, y, b);
	if (bsodstop % 2) {
		y += lock->fbsodsmall->height;
		snprintf(b, sizeof(b), "What failed: %s", bsodfails[(bsodseed >> 4) % LEN(bsodfails)]);
		bsodtext(dpy, lock, lock->fbsodsmall, tx, y, b);
	}
}

static void
draw(Display *dpy, struct xrandr *rr, struct lock *lock, int color, unsigned int fails)
{
	XRRMonitorInfo *mons = NULL;
	int nmon = 0, i;

	if (panicking && !panicbsod) {
		drawpanic(dpy, lock);
	} else {
		if (panicking) {
			XSetForeground(dpy, lock->gc, lock->bsodpixel);
			XFillRectangle(dpy, lock->buf, lock->gc, 0, 0, lock->w, lock->h);
		} else {
			XCopyArea(dpy, lock->bg, lock->buf, lock->gc, 0, 0, lock->w, lock->h, 0, 0);
		}
		if (rr->active)
			mons = XRRGetMonitors(dpy, lock->root, True, &nmon);
		if (!mons || nmon < 1) {
			if (panicking)
				drawbsodmon(dpy, lock, 0, 0, lock->w, lock->h);
			else
				drawmonitor(dpy, lock, 0, 0, lock->w, lock->h, color, fails);
		}
		for (i = 0; mons && i < nmon; i++) {
			if (panicking)
				drawbsodmon(dpy, lock, mons[i].x, mons[i].y, mons[i].width, mons[i].height);
			else
				drawmonitor(dpy, lock, mons[i].x, mons[i].y,
				            mons[i].width, mons[i].height, color, fails);
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
			if (panicstep())
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
	lock->fbsodbig = XftFontOpenName(dpy, lock->screen, bsodfontbig);
	lock->fbsod = XftFontOpenName(dpy, lock->screen, bsodfont);
	lock->fbsodsmall = XftFontOpenName(dpy, lock->screen, bsodfontsmall);
	if (!lock->ftime || !lock->fdate || !lock->finfo || !lock->fpanic ||
	    !lock->fbsodbig || !lock->fbsod || !lock->fbsodsmall)
		die("slock: cannot load fonts\n");
	XftColorAllocName(dpy, lock->vis, lock->cmap, timecolor, &lock->timec);
	XftColorAllocName(dpy, lock->vis, lock->cmap, datecolor, &lock->datec);
	XftColorAllocName(dpy, lock->vis, lock->cmap, colorname[FAILED], &lock->failc);
	XftColorAllocName(dpy, lock->vis, lock->cmap, "#FFFFFF", &lock->whitec);
	for (i = 0; i < PC_NUM; i++)
		XftColorAllocName(dpy, lock->vis, lock->cmap, panicpalette[i], &lock->pc[i]);
	XAllocNamedColor(dpy, lock->cmap, bsodcolor, &color, &dummy);
	lock->bsodpixel = color.pixel;
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
