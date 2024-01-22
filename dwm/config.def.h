/* See LICENSE file for copyright and license details. */

#include "tcl.c"

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int gappx     = 2;        /* gap pixel between windows */
static const unsigned int snap      = 1;        /* snap pixel */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0;    /* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;        /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int focusonwheel       = 0;

static const char *fonts[] = {
	"Wuncon Siji:size=12",
	"WeatherIcons:size=10",
	"UbuntuMono Nerd Font:size=10",
	"Font Awesome 5 Pro:size=10",
	"MaterialIcons:size=10",
	"xos4 Terminus:size=10"
};
static const char dmenufont[] = "xos4 Terminus:size=10";

 static const char col_text1[] = "#80CBC4";
 static const char col_text2[] = "#EEFFFF";
 static const char col_surface1[] = "#263238";
 static const char col_surface2[] = "#263238";
 static const char col_surface3[] = "#546E7A";
static const char *colors[][3] = {
	/*               fg         bg         border   */
	[SchemeNorm] = {col_text2, col_surface2, col_surface3},
	[SchemeSel] =  {col_text1, col_surface1, col_surface1},
};

/* tagging */
static const char *tags[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Kitty",    NULL,       NULL,       0,            0,           -1 },
	// { "Brave",    NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact = 0.55;	 /* factor of master area size [0.05..0.95] */
static const int nmaster = 1;		 /* number of clients in master area */
static const int resizehints = 1;	 /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{"\uE005"			, tile},
	{"\uE011"			, NULL},  /* Floating */
	{"[M]"				, monocle},
	{"|||"				, tcl },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY, TAG)\
	{MODKEY, KEY, view, {.ui = 1 << TAG}},\
	{MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << TAG}},\
	{MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},\
	{MODKEY | ControlMask | ShiftMask, KEY, toggletag, {.ui = 1 << TAG}},

#define XF86MonBrightnessDown 0x1008ff02
#define XF86MonBrightnessUp 0x1008ff03
#define XF86AudioLowerVolume 0x1008ff11
#define XF86AudioMute 0x1008ff12
#define XF86AudioRaiseVolume 0x1008ff13

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_surface2, "-nf", col_text2, "-sb", col_surface1, "-sf", col_text1, NULL };
static const char *termcmd[]  = { "kitty", NULL };
#include "shift-tools.c"

static const Key keys[] = {
	/* modifier                     key         					function  	     	       			argument */
	// Move between workspaces
	{MODKEY							, XK_bracketleft	 			, shiftviewclients 					, {.i = -1}},
	{MODKEY							, XK_bracketright 				, shiftviewclients 					, {.i = +1}},
	{MODKEY | ShiftMask				, XK_bracketleft	 			, shiftview 						, {.i = -1}},
	{MODKEY | ShiftMask				, XK_bracketright 				, shiftview 						, {.i = +1}},

	// Spawn applications
	{MODKEY							, XK_p 							, spawn 							, {.v = dmenucmd}},
	{MODKEY							, XK_space 						, spawn								, SHCMD("rofi -show drun")},
	{MODKEY | ControlMask , XK_space 						, spawn								, SHCMD("rofi -show window")},
	{MODKEY							, XK_Return 					, spawn 							, {.v = termcmd}},

	// Toggle bar
	{MODKEY							, XK_b 							, togglebar							, {0}},

	// Focus between windows
	{MODKEY							, XK_j 							, focusstack 						, {.i = +1}},
	{MODKEY							, XK_k 							, focusstack 						, {.i = -1}},

	// Move windows into master
	{MODKEY							, XK_i 							, incnmaster 						, {.i = +1}},
	{MODKEY							, XK_d 							, incnmaster 						, {.i = -1}},

	// Increase/Decrease windows left-right
	{MODKEY							, XK_h 							, setmfact 							, {.f = -0.05}},
	{MODKEY							, XK_l 							, setmfact 							, {.f = +0.05}},

	// Move window to workspaces
	{MODKEY | ShiftMask				, XK_h 							, shiftboth 						, {.i = -1}},
	{MODKEY | ShiftMask				, XK_l 							, shiftboth 						, {.i = +1}},
	{MODKEY | ControlMask			, XK_h 							, shiftswaptags 					, {.i = -1}},
	{MODKEY | ControlMask			, XK_l 							, shiftswaptags 					, {.i = +1}},

	// Zoom into master
	{MODKEY | ShiftMask				, XK_Return 					, zoom									, {0}},
	{MODKEY							, XK_Tab 						, view									, {0}},

	// Toggle modes
	{MODKEY							, XK_t 							, setlayout 						, {.v = &layouts[0]}},
	{MODKEY							, XK_f 							, setlayout 						, {.v = &layouts[1]}},
	{MODKEY							, XK_m 							, setlayout 						, {.v = &layouts[2]}},
	{MODKEY							, XK_n 							, setlayout 						, {.v = &layouts[3]}},
	{MODKEY | ControlMask			, XK_space 						, setlayout							, {0}},
	{MODKEY | ShiftMask				, XK_space 						, togglefloating					, {0}},

	// Weird shit
	{MODKEY							, XK_0 							, view 								, {.ui = ~0}},
	{MODKEY | ShiftMask				, XK_0 							, tag 								, {.ui = ~0}},

	// Move and focus between workspaces
	{MODKEY							, XK_comma 						, focusmon 							, {.i = -1}},
	{MODKEY							, XK_period 					, focusmon 							, {.i = +1}},
	{MODKEY | ShiftMask				, XK_comma 						, tagmon 							, {.i = -1}},
	{MODKEY | ShiftMask				, XK_period 					, tagmon 							, {.i = +1}},

	// Quit window
	{MODKEY | ShiftMask				, XK_q 							, killclient						, {0}},

	{0								, XF86MonBrightnessDown 		, spawn								, SHCMD("xbacklight -inc 5")},
	{0								, XF86MonBrightnessUp 			, spawn								, SHCMD("xbacklight -dec 5")},
	{0								, XF86AudioLowerVolume 			, spawn								, SHCMD("pamixer --decrease 5")},
	{0								, XF86AudioMute 				, spawn								, SHCMD("pamixer --toggle-mute")},
	{0								, XF86AudioRaiseVolume 			, spawn								, SHCMD("pamixer --increase 5")},
	// XF86AudioMicMute
	//{0								, XF86AudioMicMute				, spawning							, SHCMD ("pamixer --default-source --toggle-mute")},

	TAGKEYS(XK_1, 0)
	TAGKEYS(XK_2, 1)
	TAGKEYS(XK_3, 2)
	TAGKEYS(XK_4, 3)
	TAGKEYS(XK_5, 4)
	TAGKEYS(XK_6, 5)
	TAGKEYS(XK_7, 6)
	TAGKEYS(XK_8, 7)
	TAGKEYS(XK_9, 8)
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click            event mask  button      function        		argument */
	{ClkTagBar			, MODKEY	, Button1	, tag					, {0}},
	{ClkTagBar			, MODKEY	, Button3	, toggletag				, {0}},
	{ClkWinTitle		, 0			, Button2	, zoom					, {0}},
	{ClkStatusText		, 0			, Button2	, spawn					, {.v = termcmd}},
	/* placemouse options, choose which feels more natural:
	 *    0 - tiled position is relative to mouse cursor
	 *    1 - tiled position is relative to window center
	 *    2 - mouse pointer warps to window center
	 *
	 * The moveorplace uses movemouse or placemouse depending on the floating state
	 * of the selected client. Set up individual keybindings for the two if you want
	 * to control these separately (i.e. to retain the feature to move a tiled window
	 * into a floating position).
	 */
	{ClkClientWin		, MODKEY	, Button1	, moveorplace			, {.i = 2} },
	{ClkClientWin		, MODKEY	, Button2	, togglefloating		, {0}},
	{ClkClientWin		, MODKEY	, Button3	, resizemouse			, {0}},
	{ClkTagBar			, 0			, Button1	, view					, {0}},
	{ClkTagBar			, 0			, Button3	, toggleview			, {0}},
	{ClkTagBar			, MODKEY	, Button1	, tag					, {0}},
	{ClkTagBar			, MODKEY	, Button3	, toggletag				, {0}},
};
