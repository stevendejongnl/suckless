/* See LICENSE file for copyright and license details. */

#define SESSION_FILE "/tmp/dwm-session"

/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int snap      = 1;       /* snap pixel */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0;    /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;        /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int focusonwheel       = 0;
static const char *fonts[]          = { "monospace:size=10" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#EF476F";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
	[SchemeStatus]  = { col_gray3, col_gray1,  "#1B9AAA"  }, // Statusbar right {text,background,not used but cannot be empty}
	[SchemeTagsSel]  = { col_gray4, col_cyan,  "#06D6A0"  }, // Tagbar left selected {text,background,not used but cannot be empty}
	[SchemeTagsNorm]  = { col_gray3, col_gray1,  "#1B9AAA"  }, // Tagbar left unselected {text,background,not used but cannot be empty}
	[SchemeInfoSel]  = { col_gray4, col_cyan,  "#EF476F"  }, // infobar middle  selected {text,background,not used but cannot be empty}
	[SchemeInfoNorm]  = { col_gray3, col_gray1,  "#1B9AAA"  }, // infobar middle  unselected {text,background,not used but cannot be empty}
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
    {"Gcr-prompter",    NULL,       NULL,                   ~0,         0,              -1},
    {"Emulator",        NULL,       NULL,                   ~0,         0,              -1},
    {"Lxpolkit",        NULL,       NULL,                   ~0,         0,              -1},
    {"1Password",       NULL,       NULL,                   ~0,         0,              -1},
    {"slack",           NULL,       NULL,                   1 << 8,     0,              -1},
    {NULL,              NULL,       "Picture in picture",   ~0,         1,              -1},
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                           KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY | ControlMask,             KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY | ShiftMask,               KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY | ControlMask | ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

#define XF86MonBrightnessDown 0x1008ff02
#define XF86MonBrightnessUp 0x1008ff03
#define XF86AudioLowerVolume 0x1008ff11
#define XF86AudioMute 0x1008ff12
#define XF86AudioRaiseVolume 0x1008ff13
#define XF86Messenger 0x1008ff8e
#define XF86Favorite 0x1008ff30
#define XF86WLAN 0x1008ff95

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "alacritty", "-e", "tmux", "new-session", "-A", "-s", "main", NULL };
static const char *termtwocmd[]  = { "alacritty", "-e", "tmux", NULL };
#include "shift-tools.c"

static const Key keys[] = {
	/* modifier                     key        function        argument */
	// Move between workspaces
	{ MODKEY,		                    XK_bracketleft,   shiftviewclients,   { .i = -1 } },
	{ MODKEY,                       XK_bracketright,  shiftviewclients,   { .i = +1 } },
	{ MODKEY|ShiftMask,             XK_bracketleft, 	shiftview,          { .i = -1 } },
	{ MODKEY|ShiftMask,             XK_bracketright,	shiftview,          { .i = +1 } },

	// Spawn applications
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
  { MODKEY,                       XK_space,  spawn,          SHCMD("rofi -show drun") },
  { MODKEY|Mod1Mask,              XK_space,  spawn,          SHCMD("rofi -show window") },
	{ MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY|Mod1Mask,              XK_Return, spawn,          {.v = termtwocmd } },

	// Toggle bar
	{ MODKEY,                       XK_b,      togglebar,      {0} },

	// Focus between windows
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },

	// Increase/decrease number of master windows
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },

	// Increase/decrease master size
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY|ShiftMask,		XK_h,      shiftboth,      { .i = -1 }	},
	{ MODKEY|ControlMask,		XK_h,      shiftswaptags,  { .i = -1 }	},
	{ MODKEY|ControlMask,		XK_l,      shiftswaptags,  { .i = +1 }	},
	{ MODKEY|ShiftMask,             XK_l,      shiftboth,      { .i = +1 }	},
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },

	// Zoom into master
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },

	// Quit window
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },

	// Toggle modes
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	//{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ControlMask,           XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },

	// Show all windows
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },

	// Pin window to move active tag
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },

	// Move and focus between tags
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },

	// Function keys
	{0, XF86MonBrightnessDown, spawn, SHCMD("xbacklight -inc 5")},
	{0, XF86MonBrightnessUp, spawn, SHCMD("xbacklight -dec 5")},
	{0, XF86AudioLowerVolume, spawn, SHCMD("pamixer --decrease 5")},
	{0, XF86AudioMute, spawn, SHCMD("pamixer --toggle-mute")},
	{0, XF86AudioRaiseVolume, spawn, SHCMD("pamixer --increase 5")},
	// {0, XF86AudioMicMute, spawning, SHCMD ("pamixer --default-source --toggle-mute")},
	// {0, XF86Messenger, spawn, SHCMD("")},
	// {0, XF86Favorite, spawn, SHCMD("")},
	// {0, XF86WLAN, spawn, SHCMD("")},
  
	// Tag keys
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)

	// Quit DWM
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_q,      quit,           {1} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
