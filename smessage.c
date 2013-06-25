/* See LICENSE file for copyright and license details.
 *
 * TODO: replace the code to unselect when leaving by code
 * to unselect when window focus is lost.
 * Make pointer selection possible.
 */
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

enum {
	BG,
	FG,
	COLORS,

	OK = 0,
	CANCEL,
	BUTTONS,

	BUFSIZE = 4096
};

typedef unsigned int	uint;
typedef unsigned long	ulong;

typedef struct Button	Button;
typedef struct DC	DC;
typedef struct Line	Line;

struct Button {
	int retval;
	int xpos;
	int ypos;
	int width;
	int height;
	uint labellen;
	char *label;
	Bool pressed;
	Bool hovered;
	Bool selected;
};

struct DC {
	ulong selcolor[COLORS];
	ulong normcolor[COLORS];
	ulong presscolor[COLORS];
	ulong hovercolor[COLORS];

	GC gc;
	struct {
		int ascent;
		int descent;
		int height;
		int leading;
		XFontSet set;
		XFontStruct *fstruct;
	} font;
};

struct Line {
	uint length;
	char *text;
	Line *next;
};

#include "config.h"

static void	buttonhover(XEvent *e);
static void	buttonpress(XEvent *e);
static void	buttonrelease(XEvent *e);
static void	cleanup(void);
static void	draw(void);
static void	drawbuttons(void);
static void	drawmesg(void);
static void	drawstring(int x, int y, char *s, int l);
static void	freelines(void);
static ulong	getcolor(const char *colorname);
static void	handlekey(XEvent *e);
static Button	*hovers(int x, int y);
static void	initbuttons(void);
static void	initfont(void);
static void	leavewindow(void);
static void	makelines(void);
static void	panic(const char *errmesg, ...);
static void	run(void);
static void	setup(void);
static void	usage(int retval);

static char	*name   = "smessage";
static Bool	urgent  = False;
static Bool	confirm = False;

static Button	*buttons;
static DC	dc;
static Display	*dpy;
static Line	*lines;
static char	*message, *title;
static Window	root, win;
static int	screen, nbuttons;
static Bool	windowactive;
static Atom	wmdelmsg;

int
main(int argc, char *argv[])
{
	int	i;
	FILE	*fin;

	title = argv[0];

	/* TODO options:
	 * options to force geometry
         */
	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-v")) {		/* print version */
			puts("smessage-"VERSION", © 2012 Florian Limberger");
			exit(EXIT_SUCCESS);
		}
		else if (!strcmp(argv[i], "-h"))	/* print help */
			usage(EXIT_SUCCESS);
		else if (!strcmp(argv[i], "-u"))	/* set urgency */
			urgent = True;
		else if (!strcmp(argv[i], "-t"))	/* set title */
			title = argv[++i];
		else if (!strcmp(argv[i], "-c"))	/* confirm or deny */
			confirm = True;
		else if (argv[i][0] == '-')
			usage(EXIT_FAILURE);
		else
			break;

	if (i < argc) {
		fin = fopen(argv[i], "r");
		if (fin == NULL)
			panic("Failed to open file %s.\n", argv[i]);
	} else
		fin = stdin;

	message = (char *) malloc(BUFSIZE);
	fread(message, sizeof(char), BUFSIZE, fin);

	if (fin != stdin)
		fclose(fin);

	setup();
	run();

	/* not reached */
	return 0;
}

/*
 * Sets up resources for the X window system.
 * Cleanup is performed by the exit()-function.
 */
void
setup(void)
{
	uint	m;
	XWMHints	*wmh;
	XClassHint	*ch;
	XTextProperty	tp;

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");

	if (!(dpy = XOpenDisplay(NULL)))
		panic("Failed to open default display.\n");

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	dc.selcolor[BG] = getcolor(selbgcolor);
	dc.selcolor[FG] = getcolor(selfgcolor);
	dc.normcolor[BG] = getcolor(normbgcolor);
	dc.normcolor[FG] = getcolor(normfgcolor);
	dc.presscolor[BG] = getcolor(pressbgcolor);
	dc.presscolor[FG] = getcolor(pressfgcolor);
	dc.hovercolor[BG] = getcolor(hoverbgcolor);
	dc.hovercolor[FG] = getcolor(hoverfgcolor);

	initfont();
	makelines();

	m = DisplayHeight(dpy, screen);
	if (height > m)
		height = m;

	m = DisplayWidth(dpy, screen);
	if (width > m)
		width = m;

	dc.gc = XCreateGC(dpy, root, 0, 0);
	if (!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.fstruct->fid);

	win = XCreateSimpleWindow(dpy, root, xpos, ypos,
					width, height,
					0, 0, dc.normcolor[BG]);

	if (XStringListToTextProperty(&title, 1, &tp) == 0)
		panic("XStringToTextProperty - out of memory.\n");

	wmh = XAllocWMHints();
	wmh->input = True;
	wmh->flags = InputHint;
	if (urgent)
		wmh->flags |= XUrgencyHint;

	ch = XAllocClassHint();
	ch->res_class = name;
	ch->res_name  = name;

	XSetWMProperties(dpy, win, &tp, &tp, NULL, 0, NULL, wmh, ch);

	XFree(wmh);
	XFree(ch);
	XFree(tp.value);

	initbuttons();

	XSelectInput(dpy, win, ExposureMask | ButtonPressMask | ButtonReleaseMask |
			PointerMotionMask | KeyPressMask | LeaveWindowMask);

	/* TODO: if window active ... */
	windowactive = True;
	wmdelmsg = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmdelmsg, 1);

	XMapWindow(dpy, win);
	draw();
	atexit(cleanup);
}

/*
 * Cleans up ressources of for the XWindowSystem.
 */
void
cleanup(void)
{
	if (dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	else
		XFreeFont(dpy, dc.font.fstruct);
	XFreeGC(dpy, dc.gc);
	XDestroyWindow(dpy, win);
	XSync(dpy, False);
	freelines();
	free(buttons);
	XCloseDisplay(dpy);
}

/*
 * Returns value of the colormap entry for the given color name.
 */
ulong
getcolor(const char *colorname)
{
	Colormap cm = DefaultColormap(dpy, screen);
	XColor color, exact;

	if (XAllocNamedColor(dpy, cm, colorname, &color, &exact) == 0)
		panic("Failed to allocate color %s.\n");
	return color.pixel;
}

/*
 * Initialize the font section of the drawing context.
 */
void
initfont(void)
{
	int	i, cnt;
	char	*def, **lst;
	XRectangle	ink, log;
	XFontStruct	**fsl;

	lst = NULL;
	dc.font.set = XCreateFontSet(dpy, fontname, &lst, &cnt, &def);
	if (lst) {
		while (cnt--)
			fprintf(stderr, "warning: missing fontset: %s\n", lst[cnt]);
		XFreeStringList(lst);
	}
	if (dc.font.set) {
		cnt = XFontsOfFontSet(dc.font.set, &fsl, &lst);
		dc.font.ascent = dc.font.descent = 0;
		for (i = 0; i < cnt; i++, lst++) {
			if ((*fsl)->ascent > dc.font.ascent)
				dc.font.ascent = (*fsl)->ascent;
			if ((*fsl)->descent > dc.font.descent)
				dc.font.descent = (*fsl)->descent;
		}
	} else {
		if (!(dc.font.fstruct = XLoadQueryFont(dpy, fontname)))
			panic("Failed to load font %s.\n", fontname);
		dc.font.ascent = dc.font.fstruct->ascent;
		dc.font.descent = dc.font.fstruct->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;

	/*
	 * TODO: find more general solution
	 * Might have to be changed if the font size is changed.
	 * Also, inaccurate math!
	 */
	dc.font.leading = dc.font.height / 5;

	/*
	 * TODO: find more general solution
	 * Only works for fixed fonts.
	 */
	if (dc.font.set) {
		i = XmbTextExtents(dc.font.set, "m", sizeof("m"), &ink, &log);
	}
	printf("font: ascent: %d, descent: %d, height: %d\n", dc.font.ascent, dc.font.descent, dc.font.height);
	printf("i: %d\n", i);
	printf("ink: x: %d, y: %d, w: %d, h: %d\n", ink.x, ink.y, ink.width, ink.height);
	printf("log: x: %d, y: %d, w: %d, h: %d\n", log.x, log.y, log.width, log.height);
}

/*
 * Initialize buttons.
 */
void
initbuttons(void)
{
	if (confirm) {
		nbuttons = BUTTONS;
		buttons = (Button *) calloc(nbuttons, sizeof(Button));
		
		buttons[CANCEL].retval = EXIT_FAILURE;
		buttons[CANCEL].xpos   = 0;
		buttons[CANCEL].width  = width / 2;
		buttons[CANCEL].height = dc.font.height;
		buttons[CANCEL].ypos   = height - buttons[CANCEL].height;

		buttons[CANCEL].label    = "Cancel";
		buttons[CANCEL].labellen = strlen(buttons[CANCEL].label);
		buttons[CANCEL].pressed  = False;
		buttons[CANCEL].hovered  = False;
		buttons[CANCEL].selected = False;

		buttons[OK].retval = EXIT_SUCCESS;
		buttons[OK].xpos   = buttons[CANCEL].width;
		buttons[OK].width  = width - buttons[CANCEL].width;
		buttons[OK].height = dc.font.height;
		buttons[OK].ypos   = height - buttons[OK].height;

		buttons[OK].label    = "Ok";
		buttons[OK].labellen = strlen(buttons[OK].label);
		buttons[OK].pressed  = False;
		buttons[OK].hovered  = False;
		buttons[OK].selected = True;
	} else {
		nbuttons = 1;
		buttons = (Button *) malloc(sizeof(Button));

		buttons->retval = EXIT_SUCCESS;
		buttons->xpos   = 0;
		buttons->width  = width;
		buttons->height = dc.font.height;
		buttons->ypos   = height - buttons->height;

		buttons->label    = "Ok";
		buttons->labellen = strlen(buttons->label);
		buttons->pressed  = False;
		buttons->hovered  = False;
		buttons->selected = True;
	}
}

/*
 * Make list of lines to draw.
 * CAUTION: must be called before the first call of draw()
 * and after the font initialisation.
 */
void
makelines(void)
{
	char	*c;
	int	i, mc, ml;
	Line	*l, *m;

	lines = (Line *) malloc(sizeof(Line));
	lines->text = message;
	lines->next = NULL;

	mc = ml = 0;
	m = lines;
	for (i = 0, c = message; *c; c++)
		if (*c == '\n') {
			*c = '\0';
			m->length = i;
			if (i > mc)
				mc = i;
			i = 0;

			if (*(c + 1)) {
				l = (Line *) malloc(sizeof(Line));
				l->text = c + 1;
				l->next = NULL;
				m->next = l;
				m = l;
				ml++;
			}
		} else
			i++;

	if (height == 0)
		height = ml * (dc.font.height + dc.font.leading) -
				(dc.font.height + 3 * margin);

	/* TODO: font width is not always fixed and not always 8! */
	if (width == 0)
		width = mc * 8 - 2 * margin;

}

/*
 * Free the list of lines.
 */
void
freelines(void)
{
	Line	*l, *m;

	for (l = lines; l->next;) {
		m = l->next;
		free(l);
		l = m;
	}
}

/*
 * Updates the geometry and calls the actual drawing functions.
 */
void
draw(void)
{
	uint	pb, pd;
	Window	pwin;

	XGetGeometry(dpy, win, &pwin, &xpos, &ypos, &width, &height, &pb, &pd);

	if (confirm) {
		buttons[CANCEL].xpos = 0;
		buttons[CANCEL].ypos = height - buttons[CANCEL].height;
		buttons[CANCEL].width = width / 2;

		buttons[OK].xpos = buttons[CANCEL].width;
		buttons[OK].ypos = height - buttons[OK].height;
		buttons[OK].width = width / 2;
	} else {
		buttons->xpos = 0;
		buttons->ypos = height - buttons->height;
		buttons->width = width;
	}
	drawmesg();
	drawbuttons();
}

/*
 * Draws the message.
 * TODO: scrolling instead of wrapping/cropping?
 */
void
drawmesg(void)
{
	int	dly;
	uint	i, j, mc, ml;
	Line	*l;

	dly = (dc.font.height + dc.font.leading);

	/* fucked by i18n: dc.font->max_bounds.width
	 * TODO: find a less ugly solution! */
	mc = (width - 2 * margin) / 8;
	ml = (height - 3 * margin - buttons->height) / dly;

	if (mc <= 0 || ml <= 0)
		return;

	for (i = 0, l = lines; l && i <= ml; i++, l = l->next) {
		if (l->length > mc) {
			for (j = 0; j < l->length; j += mc, i++) {
				drawstring(margin, dly * (i + 1), l->text + j,
				(l->length - j) > mc ? mc : l->length - j);
			}
			i--;
		} else
			drawstring(margin, dly * (i + 1), l->text, l->length);
	}
}

/*
 * Draw a string to the display.
 */
void
drawstring(int x, int y, char *s, int l)
{
	if (dc.font.set)
		XmbDrawString(dpy, win, dc.font.set, dc.gc, x, y, s, l);
	else
		XDrawString(dpy, win, dc.gc, x, y, s, l);
}

/*
 * Draws the buttons.
 */
void
drawbuttons(void)
{
	int	i;
	ulong	*color;
	XRectangle	r;

	for (i = 0; i < nbuttons; i++) {
		r.x = buttons[i].xpos;
		r.y = buttons[i].ypos;
		r.width  = buttons[i].width;
		r.height = buttons[i].height;

		if (buttons[i].pressed)
			color = dc.presscolor;
		else if (buttons[i].hovered)
			color = dc.hovercolor;
		else if (windowactive && buttons[i].selected)
			color = dc.selcolor;
		else
			color = dc.normcolor;

		XSetForeground(dpy, dc.gc, color[BG]);

		XFillRectangles(dpy, win, dc.gc, &r, 1);

		XSetForeground(dpy, dc.gc, color[FG]);
		drawstring(buttons[i].xpos + buttons[i].width / 2,
				height - dc.font.descent,
				buttons[i].label, buttons[i].labellen);
	}

	XSync(dpy, False);
}

/*
 * Main event loop.
 */
void
run(void)
{
	XEvent	e;

	for (;;) {
		XNextEvent(dpy, &e);
		switch (e.type) {
		case ClientMessage:
			if ((Atom) e.xclient.data.l[0] == wmdelmsg)
				exit(EXIT_SUCCESS);
		case Expose:
			if (e.xexpose.count == 0)
				draw();
				break;
		case KeyPress:
			handlekey(&e);
			break;
		case ButtonPress:
			buttonpress(&e);
			break;
		case ButtonRelease:
			buttonrelease(&e);
			break;
		case MotionNotify:
			buttonhover(&e);
			break;
		case LeaveNotify:
			leavewindow();
			break;
		default:
			break;
		}
	}
}

/*
 * Handle key pressing events.
 */
void
handlekey(XEvent *e)
{
	KeySym	key;

	key = XkbKeycodeToKeysym(dpy, e->xkey.keycode, 0, 0);
	switch (key) {
	case XK_Escape:
		if (confirm)
			exit(buttons[CANCEL].retval);
		break;
	case XK_Return:
		if (confirm) {
			if (buttons[CANCEL].selected)
				exit(buttons[CANCEL].retval);
			else
				exit(buttons[OK].retval);
		} else
			exit(buttons->retval);
		break;
	case XK_Left:
		if (confirm) {
			buttons[CANCEL].selected = True;
			buttons[OK].selected = False;
			drawbuttons();
		}
		break;
	case XK_Right:
		if (confirm) {
			buttons[CANCEL].selected = False;
			buttons[OK].selected = True;
			drawbuttons();
		}
		break;
	}
}

/*
 * Handle mouse moving events.
 */
void
buttonhover(XEvent *e)
{
	int	i;
	Button	*b;

	windowactive = True;
	b = hovers(e->xmotion.x, e->xmotion.y);
	if (b != NULL)
		b->hovered = True;
	else {
		for (i = 0; i < nbuttons; i++) {
			buttons[i].pressed = False;
			buttons[i].hovered = False;
		}
	}
	drawbuttons();
}

/*
 * Handle mouse button press events.
 */
void
buttonpress(XEvent *e)
{
	Button	*b;

	if (e->xbutton.button != Button1)
		return;

	b = hovers(e->xbutton.x, e->xbutton.y);
	if (b != NULL) {
		b->pressed = True;
		drawbuttons();
	}
}

/*
 * Handle mouse button release events.
 */
void
buttonrelease(XEvent *e)
{
	Button	*b;

	if (e->xbutton.button != Button1)
		return;

	b = hovers(e->xbutton.x, e->xbutton.y);
	if (b != NULL && b->pressed) {
		exit(b->retval);
	}
}

/*
 * Handle mouse leaving window events.
 * TODO: really needed? Replace by window focus lost event.
 */
void
leavewindow(void)
{
	windowactive = False;
	drawbuttons();
}

/*
 * Returns the button which is at the given coordinates or NULL if there is none.
 */
Button *
hovers(int x, int y)
{
	int	i;

	for (i = 0; i < nbuttons; i++)
		if (x > buttons[i].xpos &&
			x < (buttons[i].xpos + buttons[i].width) &&
			y > buttons[i].ypos &&
			y < (buttons[i].ypos + buttons[i].height))
			return &buttons[i];
	return NULL;
}

/*
 * Print usage information and exit application with given exit status.
 */
void
usage(int retval)
{
	FILE	*out;

	if (retval != EXIT_SUCCESS)
		out = stderr;
	else
		out = stdout;

	fprintf(out, "usage: %s [-v] [-h] [-c] [-u] [-t title] [FILE]\n", name);

	exit(retval);
}

/*
 * Prints formatted error message to stderr and exits with EXIT_FAILURE.
 */
void
panic(const char *errmesg, ...)
{
	va_list	ap;

	va_start(ap, errmesg);
	vfprintf(stderr, errmesg, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}
