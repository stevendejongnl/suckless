/* user and group to drop privileges to */
static const char *user  = "stevendejong";
static const char *group = "stevendejong";

/* Rime theme, same colors as the dwm status bar */
static const char *bgcolor = "#12141C";     /* behind the wallpaper, and when it is missing */
static const char *colorname[NUMCOLS] = {
	[INIT] =   "#2A2F3D",   /* state bar after initialization */
	[INPUT] =  "#3EE6E0",   /* state bar during input */
	[FAILED] = "#FF5C78",   /* state bar and failed attempts after a wrong password */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* blurred wallpaper as a raw PPM (P6), relative to $HOME. Written by ~/.dwm/lock.sh,
 * the plain bgcolor is used when the file is missing */
static const char *bgimage = ".cache/slock/bg.ppm";

/* clock, date and the state bar, centered on every monitor */
static const char *timefont  = "JetBrainsMono Nerd Font Mono:size=80";
static const char *datefont  = "JetBrainsMono Nerd Font Mono:size=22";
static const char *infofont  = "JetBrainsMono Nerd Font Mono:size=16";
static const char *timecolor = "#D3D6E6";
static const char *datecolor = "#9AA0C0";
static const char *timefmt   = "%H:%M";
static const char *datefmt   = "%A %d %B %Y";
static const int barwidth    = 320;
static const int barheight   = 4;

/* fake kernel panic after this many seconds without a key, 0 turns it off.
 * Any key ends it and shows the lock screen again. */
static const unsigned int panicafter = 120;
static const char *panicfont      = "JetBrainsMono Nerd Font Mono:size=11";
static const char *panictextcolor = "#D3D6E6";
