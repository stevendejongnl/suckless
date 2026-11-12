/* See LICENSE file for copyright and license details. */

#define SESSION_FILE "/tmp/dwm-session"
/* last tag and monitor per window class, relative to $HOME; survives logout */
#define MEMORY_FILE ".local/state/dwm-windows"
#define SHARED_TAG_PLAYING_FILE ".local/state/dwm-shared-tag-playing"

/* appearance */
static const unsigned int borderpx  = 2;	/* border pixel of windows */
static const unsigned int snap	    = 1;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const int swterminheritfs    = 1;        /* 1 terminal inherits fullscreen on unswallow */
static const unsigned int systraypinning = 0;	/* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0;	/* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2;	/* systray spacing */
static const int systraypinningfailfirst = 1;	/* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray	    = 1;	/* 0 means no systray */
/* systray icons to refuse, matched on the WM_CLASS instance or class of the icon window.
 * Find a name with: xprop -id <icon window> WM_CLASS. Keep NULL as the last entry. */
static const char *systrayignore[] = {
	"indicator-solaar",
	NULL
};
/* status bar chips (colour escapes 0x02-0x05, 0x0D, 0x0E): a rounded box inset from the top and bottom of the bar */
static const int chipinset	    = 2;	/* pixels above and below the chip */
static const int chipradius	    = 4;	/* corner radius, 0 means square */
static const int showbar	    = 1;	/* 0 means no bar */
static const int topbar		    = 1;	/* 0 means bottom bar */
static const int focusonwheel	    = 0;
static const char *fonts[]	    = {
	"monospace:size=10",
	"JetBrainsMono Nerd Font Mono:size=10",
	"Noto Color Emoji:size=10"
};
static const char dmenufont[]	    = "monospace:size=10,Noto Color Emoji:size=10";
static const char col_gray1[]	    = "#12141C";
static const char col_gray2[]	    = "#2A2F3D";
static const char col_gray3[]	    = "#7A7F99";
static const char col_gray4[]	    = "#D3D6E6";
static const char col_cyan[]	    = "#3EE6E0";
/* Status bar colors — Rime theme, https://git.madebysteven.nl/stevendejong/rime */
static const char col_status_norm[]	= "#9AA0C0";  /* Default status text */
static const char col_status_low[]	= "#3EE6E0";  /* Low/info - cyan */
static const char col_status_medium[]	= "#FFD473";  /* Medium - yellow */
static const char col_status_high[]	= "#FFA36B";  /* High/warning - orange */
static const char col_status_critical[] = "#FF5C78";  /* Critical - red/pink */
static const char col_status_success[]	= "#7FD88F";  /* Success - green */
static const char *colors[][3]	    = {
	/*		 fg	    bg	       border	*/
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan	 },
	[SchemeStatus]	= { col_gray3, col_gray1,  "#3EE6E0"  }, // Statusbar right {text,background,not used but cannot be empty}
	[SchemeTagsSel]	 = { col_cyan, col_gray1,  "#7FD88F"  }, // Tagbar left selected {text,background,not used but cannot be empty}
	[SchemeTagsNorm]  = { col_gray3, col_gray1,  "#3EE6E0"	}, // Tagbar left unselected {text,background,not used but cannot be empty}
	[SchemeInfoSel]	 = { col_cyan, col_gray1,  "#FF5C78"  }, // infobar middle  selected {text,background,not used but cannot be empty}
	[SchemeInfoNorm]  = { col_gray3, col_gray1,  "#3EE6E0"	}, // infobar middle  unselected {text,background,not used but cannot be empty}
	/* Status bar color schemes */
	[SchemeStatusNorm]     = { col_status_norm,	col_gray1, col_gray2 },
	[SchemeStatusLow]      = { col_status_low,	col_gray1, col_gray2 },
	[SchemeStatusMedium]   = { col_status_medium,	col_gray1, col_gray2 },
	[SchemeStatusHigh]     = { col_status_high,	col_gray1, col_gray2 },
	[SchemeStatusCritical] = { col_status_critical, col_gray1, col_gray2 },
	[SchemeStatusSuccess]  = { col_status_success,	col_gray1, col_gray2 },
	[SchemeStatusBadgeOn]  = { col_gray1,		col_status_success, col_gray2 },
	[SchemeStatusBadgeOff] = { col_gray4,		col_gray2, col_gray2 },
	[SchemeBadgeRed]       = { col_gray1,		col_status_critical, col_gray2 },
	[SchemeStatusBadgeMedium] = { col_gray1,	col_status_medium,   col_gray2 },
	[SchemeStatusBadgeHigh]	  = { col_gray1,	col_status_high,     col_gray2 },
	[SchemeStatusBadgeSel]	  = { col_gray1,	col_cyan,	     col_gray2 }, /* the chip of the open popup tab */
	[SchemeCaseDot]		  = { "#7A7F99",	col_gray1,	     col_gray2 }, /* the PC case dot (escape 0x06), the foreground comes from ~/.config/case-color */
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "\xe2\x96\xb6" }; /* tag 9: YouTube or Spotify, play triangle (U+25B6) */
static const char scratchpadname[] = "scratchpad";

#define TAG_MASK(n) (1 << ((n) - 1))
#define SHARED_TAG TAG_MASK(9) /* YouTube and Spotify share this tag */
#define TAG_ALL (~0)
/* WM_CLASS class of apps whose popups are plain normal windows. Like dialogs they open on
 * the monitor and tags of the cursor, not where the class was last seen, and are never saved.
 * Keep NULL as the last entry. */
static const char *popupclasses[] = {
	"com.onepassword.OnePassword", /* Quick Access and the authorization prompts */
	NULL
};

/* WM_CLASS class of windows that keep the position they opened at. dwm adds the monitor origin to the
 * position a client asks for later, so on a second monitor such a window would end up in the middle of it.
 * The status bar popup (dwm-popup.py) is one: it opens under the click. Keep NULL as the last entry. */
static const char *fixedposclasses[] = {
	"claude-git-guard",
	NULL
};

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class	      instance	  title			  tags mask	   isfloating	   isterminal	noswallow  monitor */
    {scratchpadname,    NULL,      NULL,           TAG_ALL,         1,           1,           -1,        -1},
    {"Alacritty",       NULL,      NULL,           0,               0,           1,           0,         -1},
    {"Gcr-prompter",	NULL,	    NULL,		    TAG_ALL,	     0,           0,           1,         -1},
    {NULL,	"qemu-system-x86_64", NULL,	    TAG_MASK(8),     1,           0,           1,         -1},
    {NULL,	"www.youtube.com", NULL,	    SHARED_TAG,      0,           0,           1,         -1},
    {"Spotify",	NULL,	    NULL,		    SHARED_TAG,      0,           0,           1,         -1},
    {"Lxpolkit",	NULL,	    NULL,		    TAG_ALL,	     0,           0,           1,         -1},
    {"1Password",	NULL,	    NULL,		    TAG_ALL,	     0,           0,           1,         -1},
    {"Bitwarden",	NULL,	    NULL,		    TAG_ALL,	     0,           0,           1,         -1},
    {"Slack",		NULL,	    NULL,		    TAG_MASK(1),     0,           0,           1,          1},
    {"Wasistlos",	NULL,	    NULL,		    TAG_MASK(1),     0,           0,           1,          1},
    {"claude-git-guard",NULL,   NULL,               TAG_ALL,         1,           0,           1,         -1},
    {"Gkrellm",         NULL,   NULL,               TAG_ALL,         1,           0,           1,         -1},
    {"flameshot",       NULL,   NULL,               TAG_ALL,         1,           0,           1,         -1},
    {NULL,		NULL,	    "DWM Overlay",	    TAG_ALL,	     1,           0,           1,         -1},
    {NULL,		NULL,	    "Picture in picture",   TAG_ALL,	     1,           0,           1,         -1},
};

/* apps that share SHARED_TAG: when one opens, the other one closes. class and instance are substring matches.
 * The tag takes the scheme while the app is active: green for Spotify while it plays, red for YouTube. */
static const SharedTagApp sharedtagapps[] = {
	/* class	instance		tag scheme		needsplaying */
	{ NULL,		"www.youtube.com",	SchemeBadgeRed,		0 },
	{ "Spotify",	NULL,			SchemeStatusBadgeOn,	1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */
static const int refreshrate = 120;  /* refresh rate (per second) for client move/resize */

#define FORCE_VSPLIT 1	/* nrowgrid layout: force two clients to always split vertically */
#define PERTAG_PATCH 0	/* vanitygaps uses global gaps (Pertag struct not available at compile time) */

/* Gaps */
static const unsigned int gappih = 4;	/* horiz inner gap between windows */
static const unsigned int gappiv = 4;	/* vert inner gap between windows */
static const unsigned int gappoh = 4;	/* horiz outer gap between windows and screen edge */
static const unsigned int gappov = 4;	/* vert outer gap between windows and screen edge */
static const int smartgaps = 0;		/* 1 means no outer gap when there is only one window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },	 /* first entry is default */
	{ "><>",      NULL },	 /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,			    KEY,      view,	      {.ui = 1 << TAG} }, \
	{ MODKEY | ControlMask,		    KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY | ShiftMask,		    KEY,      tag,	      {.ui = 1 << TAG} }, \
	{ MODKEY | ControlMask | ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },
/* one-handed alternative: Mod+Alt+1-4 reaches tags 5-8 */
#define TAGKEYS_ALT(KEY,TAG) \
	{ MODKEY | Mod1Mask,				KEY,      view,	      {.ui = 1 << TAG} }, \
	{ MODKEY | Mod1Mask | ControlMask,		KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY | Mod1Mask | ShiftMask,		KEY,      tag,	      {.ui = 1 << TAG} }, \
	{ MODKEY | Mod1Mask | ControlMask | ShiftMask,	KEY,      toggletag,      {.ui = 1 << TAG} },

#define XF86MonBrightnessDown 0x1008ff02
#define XF86MonBrightnessUp 0x1008ff03
#define XF86AudioLowerVolume 0x1008ff11
#define XF86AudioMute 0x1008ff12
#define XF86AudioRaiseVolume 0x1008ff13
#define XF86AudioPlay 0x1008ff14
#define XF86AudioPause 0x1008ff31
#define XF86AudioPrev 0x1008ff16
#define XF86AudioNext 0x1008ff17
#define XF86Messenger 0x1008ff8e
#define XF86Favorite 0x1008ff30
#define XF86WLAN 0x1008ff95

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

#include "movestack.c"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "vanitygaps.c"
#pragma GCC diagnostic pop

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "alacritty", "-e", "/home/stevendejong/.local/bin/tmux-main", NULL };
static const char *termtwocmd[]	 = { "alacritty", "-e", "tmux", NULL };
static const char *scratchpadcmd[] = {
	"alacritty", "--class", scratchpadname,
	"-e", "tmux", "new-session", "-A", "-s", "scratch",
	NULL
};
static const char *helpoverlaycmd[] = {
    "/usr/local/bin/dwm-hotkey-overlay",
    NULL
};

static const char *notifcmd[] = { "/home/stevendejong/.dwm/statusbar/dwm-popup.py", "notifications", NULL };
#include "shift-tools.c"

static const Key keys[] = {
	/* modifier			key	   function	   argument */
	// Move between workspaces
	// Move to previous workspace
	{ MODKEY,			XK_bracketleft,	shiftviewclients,	{ .i = -1 } },
	// Move to next workspace
	{ MODKEY,			XK_bracketright,	shiftviewclients,	{ .i = +1 } },
	// Move active workspace left
	{ MODKEY|ShiftMask,		XK_bracketleft,	shiftview,		{ .i = -1 } },
	// Move active workspace right
	{ MODKEY|ShiftMask,		XK_bracketright,	shiftview,		{ .i = +1 } },

	// Spawn applications
	// Open application launcher
	{ MODKEY,			XK_p,		spawn,		{.v = dmenucmd } },
	// Open rofi app launcher
	{ MODKEY,			XK_space,	spawn,		SHCMD("rofi -show drun") },
	// Switch windows with rofi
	{ MODKEY|Mod1Mask,		XK_space,	spawn,		SHCMD("rofi -show window") },
	// Open terminal
	{ MODKEY,			XK_Return,	spawn,		{.v = termcmd } },
	// Open terminal in new window
	{ MODKEY|Mod1Mask,		XK_Return,	spawn,		{.v = termtwocmd } },
	// Toggle scratchpad
	{ MODKEY,			XK_grave,	togglescratch,	{.v = scratchpadcmd } },
  // Bitwarden Rofi
  { MODKEY|ControlMask|ShiftMask, XK_space, spawn,  SHCMD("rofi-rbw") },

	// Help overlay
	// Open keybindings help overlay
	{ ControlMask,			XK_h,		spawn,		{.v = helpoverlaycmd } },

	// Notification center
	// Show notification center
	{ MODKEY,			XK_n,		spawn,		{.v = notifcmd } },
	{ MODKEY,			XK_g,		spawn,		SHCMD("gkrellm") },

	// Toggle bar
	// Toggle status bar
	{ MODKEY,			XK_b,		togglebar,	{0} },

	// Focus between windows
	// Focus next window
	{ MODKEY,			XK_j,		focusstack,	{.i = +1 } },
	// Focus previous window
	{ MODKEY,			XK_k,		focusstack,	{.i = -1 } },

	// Move windows in stack
	// Move window down
	{ MODKEY|ShiftMask,		XK_j,		movestack,	{.i = +1 } },
	// Move window up
	{ MODKEY|ShiftMask,		XK_k,		movestack,	{.i = -1 } },

	// Increase/decrease number of master windows
	// Add master window
	{ MODKEY,			XK_i,		incnmaster,	{.i = +1 } },
	// Remove master window
	{ MODKEY,			XK_d,		incnmaster,	{.i = -1 } },

	// Increase/decrease master size
	// Decrease master size
	{ MODKEY,			XK_h,		setmfact,	{.f = -0.05} },
	// Move active master left
	{ MODKEY|ShiftMask,		XK_h,		shiftboth,	{ .i = -1 } },
	// Swap active with left master
	{ MODKEY|ControlMask,		XK_h,		shiftswaptags,	{ .i = -1 } },
	// Swap active with right master
	{ MODKEY|ControlMask,		XK_l,		shiftswaptags,	{ .i = +1 } },
	// Move active master right
	{ MODKEY|ShiftMask,		XK_l,		shiftboth,	{ .i = +1 } },
	// Increase master size
	{ MODKEY,			XK_l,		setmfact,	{.f = +0.05} },

	// Zoom into master
	// Zoom active window to master
	{ MODKEY,			XK_Return,	zoom,		{0} },
	// Toggle to previous tag (Super+Tab is taken by dwm-dock)
	{ MODKEY|ControlMask,		XK_Tab,		view,		{0} },

	// Quit window
	// Kill active window
	{ MODKEY|ShiftMask,		XK_c,		killclient,	{0} },
	// Force-kill a hung window (click the target)
	{ MODKEY|ShiftMask,		XK_x,		spawn,		SHCMD("xkill") },

	// Toggle modes
	// Set tiled layout
	{ MODKEY,			XK_t,		setlayout,	{.v = &layouts[0]} },
	// Set floating layout
	{ MODKEY,			XK_f,		setlayout,	{.v = &layouts[1]} },
	// Set monocle layout
	{ MODKEY,			XK_m,		setlayout,	{.v = &layouts[2]} },
	//{ MODKEY,			XK_space,	setlayout,	{0} },
	// Toggle between layouts
	{ MODKEY|ControlMask,		XK_space,	setlayout,	{0} },
	// Toggle floating mode
	{ MODKEY|ShiftMask,		XK_space,	togglefloating,	{0} },

	// Manage gaps
	// Increase all gaps
	{ MODKEY|Mod1Mask,		XK_u,		incrgaps,	{.i = +1} },
	// Decrease all gaps
	{ MODKEY|Mod1Mask|ShiftMask,	XK_u,		incrgaps,	{.i = -1} },
	// Increase inner gaps
	{ MODKEY|Mod1Mask,		XK_i,		incrigaps,	{.i = +1} },
	// Decrease inner gaps
	{ MODKEY|Mod1Mask|ShiftMask,	XK_i,		incrigaps,	{.i = -1} },
	// Increase outer gaps
	{ MODKEY|Mod1Mask,		XK_o,		incrogaps,	{.i = +1} },
	// Decrease outer gaps
	{ MODKEY|Mod1Mask|ShiftMask,	XK_o,		incrogaps,	{.i = -1} },
	// Toggle gaps
	{ MODKEY|Mod1Mask,		XK_6,		togglegaps,	{0} },
	// Reset gaps to default
	{ MODKEY|Mod1Mask|ShiftMask,	XK_6,		defaultgaps,	{0} },

	// Show all windows
	// Show all windows
	{ MODKEY,			XK_0,		view,		{.ui = ~0 } },

	// Pin window to move active tag
	// Tag window to all workspaces
	{ MODKEY|ShiftMask,		XK_0,		tag,		{.ui = ~0 } },

	// Move and focus between tags
	// Focus previous monitor
	{ MODKEY,			XK_comma,	focusmon,	{.i = -1 } },
	// Focus next monitor
	{ MODKEY,			XK_period,	focusmon,	{.i = +1 } },
	// Move window to previous monitor
	{ MODKEY|ShiftMask,		XK_comma,	tagmon,		{.i = -1 } },
	// Move window to next monitor
	{ MODKEY|ShiftMask,		XK_period,	tagmon,		{.i = +1 } },

	// Function keys
	// Decrease brightness
	{ 0,				XF86MonBrightnessDown, spawn, SHCMD("xbacklight -inc 5") },
	// Increase brightness
	{ 0,				XF86MonBrightnessUp, spawn, SHCMD("xbacklight -dec 5") },
	// Lower volume
	{ 0,				XF86AudioLowerVolume, spawn, SHCMD("pamixer --decrease 5") },
	// Mute audio
	{ 0,				XF86AudioMute, spawn, SHCMD("pamixer --toggle-mute") },
	// Raise volume
	{ 0,				XF86AudioRaiseVolume, spawn, SHCMD("pamixer --increase 5") },
	// Play or pause media
	{ 0,				XF86AudioPlay, spawn, SHCMD("playerctl play-pause") },
	// Pause media (headphones send pause and play as separate keys)
	{ 0,				XF86AudioPause, spawn, SHCMD("playerctl pause") },
	// Previous track
	{ 0,				XF86AudioPrev, spawn, SHCMD("playerctl previous") },
	// Next track
	{ 0,				XF86AudioNext, spawn, SHCMD("playerctl next") },
	// { 0,			XF86AudioMicMute, spawn, SHCMD ("pamixer --default-source --toggle-mute") },
	// { 0,			XF86Messenger, spawn, SHCMD("") },
	// { 0,			XF86Favorite, spawn, SHCMD("") },
	// { 0,			XF86WLAN, spawn, SHCMD("") },
	// Screenshot
	{ 0,				XK_Print,    spawn, SHCMD("flameshot gui") },

	// Tag keys
	TAGKEYS(			XK_1,			0)
	TAGKEYS(			XK_2,			1)
	TAGKEYS(			XK_3,			2)
	TAGKEYS(			XK_4,			3)
	TAGKEYS(			XK_5,			4)
	TAGKEYS(			XK_6,			5)
	TAGKEYS(			XK_7,			6)
	TAGKEYS(			XK_8,			7)
	TAGKEYS_ALT(			XK_1,			4)
	TAGKEYS_ALT(			XK_2,			5)
	TAGKEYS_ALT(			XK_3,			6)
	TAGKEYS_ALT(			XK_4,			7)

	// Tag 9: go to the open YouTube or Spotify window, or ask which one to open (yad popup)
	{ MODKEY,			XK_9,		spawn,		SHCMD("/home/stevendejong/.local/bin/tag9-media") },

	// Quit DWM
	// Quit DWM
	{ MODKEY|ShiftMask,		XK_q,		quit,		{0} },
	// Restart DWM
	{ MODKEY|ControlMask|ShiftMask,	XK_q,		quit,		{1} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click		event mask	button		function	argument */
	{ ClkLtSymbol,		0,		Button1,	setlayout,	{0} },
	{ ClkLtSymbol,		0,		Button3,	setlayout,	{.v = &layouts[2]} },
	{ ClkWinTitle,		0,		Button2,	zoom,		{0} },
	{ ClkStatusText,	0,		Button1,	sigstatusbar,	{.i = 1} },
	{ ClkStatusText,	0,		Button2,	sigstatusbar,	{.i = 2} },
	{ ClkStatusText,	0,		Button3,	sigstatusbar,	{.i = 3} },
	{ ClkClientWin,		MODKEY,		Button1,	movemouse,	{0} },
	{ ClkClientWin,		MODKEY,		Button2,	togglefloating,	{0} },
	{ ClkClientWin,		MODKEY,		Button3,	resizemouse,	{0} },
	{ ClkTagBar,		0,		Button1,	view,		{0} },
	{ ClkTagBar,		0,		Button3,	toggleview,	{0} },
	{ ClkTagBar,		MODKEY,		Button1,	tag,		{0} },
	{ ClkTagBar,		MODKEY,		Button3,	toggletag,	{0} },
};
