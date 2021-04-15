/*
 * See LICENSE file for copyright and license details.
 */

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <bsd/string.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "SafeXFetchName.h"
#include "arg.h"
#include "socket.h"

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_REQUEST_FOCUS 3
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5
#define XEMBED_FOCUS_NEXT 6
#define XEMBED_FOCUS_PREV 7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON 10
#define XEMBED_MODALITY_OFF 11
#define XEMBED_REGISTER_ACCELERATOR 12
#define XEMBED_UNREGISTER_ACCELERATOR 13
#define XEMBED_ACTIVATE_ACCELERATOR 14

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT 0
#define XEMBED_FOCUS_FIRST 1
#define XEMBED_FOCUS_LAST 2

/* Macros */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LENGTH(x) (sizeof((x)) / sizeof(*(x)))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
// To only be used with statically allocated strings
// because of the sizeof
#define TEXTW(x) (textnw(x, strnlen(x, sizeof(x))) + dc.font.height)
// Max size of string returned by XGetProperty
// https://github.com/mirror/libX11/blob/192bbb9e2fc45df4e17b35b6d14ea0eb418dbd39/src/GetProp.c#L83
// It's there to limit the problems if
#define MAX_XTEXT_SIZE (INT_MAX >> 4) * sizeof(char)

enum { ColFG, ColBG, ColLast }; /* color */
enum {
  WMProtocols,
  WMDelete,
  WMName,
  WMState,
  WMFullscreen,
  XEmbed,
  WMSelectTab,
  WMLast
}; /* default atoms */

typedef union {
  int i;
  const void *v;
} Arg;

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  int x, y, w, h;
  XftColor norm[ColLast];
  XftColor sel[ColLast];
  XftColor urg[ColLast];
  Drawable drawable;
  GC gc;
  struct {
    int ascent;
    int descent;
    int height;
    XftFont *xfont;
  } font;
} DC; /* draw context */

typedef struct {
  char name[256];
  Window win;
  int tabx;
  Bool urgent;
  Bool closed;
} Client;

typedef struct {
  int askforshellpwd;
  unsigned long current_focused_window;
  int shellpwd_written;
  int shellpwd_read;
  int running;
  unsigned long debug_time;
  char shell_pwd[256];
} SharedMemory;

/* function declarations */
#define SHARED_MEMORY_SIZE sizeof(SharedMemory)
void *create_shared_memory(size_t size);
static void buttonpress(const XEvent *e);
static void cleanup(void);
static void clientmessage(const XEvent *e);
static void configurenotify(const XEvent *e);
static void configurerequest(const XEvent *e);
static void createnotify(const XEvent *e);
static void destroynotify(const XEvent *e);
static void die(const char *errstr, ...);
static void drawbar(void);
static void drawtext(const char *text, XftColor col[ColLast], size_t size);
static void *ecalloc(size_t n, size_t size);
static void *erealloc(void *o, size_t size);
static void expose(const XEvent *e);
static void focus(int c);
static void focusin(const XEvent *e);
static void focusonce(const Arg *arg);
static void focusurgent(const Arg *arg);
static void fullscreen(const Arg *arg);
static char *getatom(int a);
static int getclient(Window w);
static XftColor getcolor(const char *colstr);
static int getfirsttab(void);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void initfont(const char *fontstr);
static Bool isprotodel(int c);
static void keypress(const XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window win);
static void maprequest(const XEvent *e);
static void move(const Arg *arg);
static void movetab(const Arg *arg);
static void propertynotify(const XEvent *e);
static void resize(int c, int w, int h);
static void rotate(const Arg *arg);
static void run(void);
static void sendxembed(int c, long msg, long detail, long d1, long d2);
static void setcmd(int argc, char *argv[], int);
static void insertcmd(char *command, size_t cmd_size, size_t pos);
static void printcmds(int debug);
static void setup(void);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void spawn_no_xembed_port(const Arg *arg);
static int textnw(const char *text, unsigned int len);
static void toggle(const Arg *arg);
static void unmanage(int c);
static void unmapnotify(const XEvent *e);
static void updatenumlockmask(void);
static void updatetitle(int c);
static int xerror(Display *dpy, XErrorEvent *ee);
static void xsettitle(Window w, const char *str);

/* variables */
static int screen;
static void (*handler[LASTEvent])(const XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest,
    [CreateNotify] = createnotify,
    [UnmapNotify] = unmapnotify,
    [DestroyNotify] = destroynotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [PropertyNotify] = propertynotify,
};
static int bh, wx, wy, ww, wh;
static unsigned int numlockmask;
static Bool running = True, nextfocus, doinitspawn = True, fillagain = False,
            closelastclient = False, killclientsfirst = False;
/* static atomic_bool askforshellpwd = True; */
/* static atomic_ulong current_focused_window; */
static SharedMemory *shared_memory = NULL;
static int log_file;
static char TABBED_LOG_FILE[] = "tabbed-log-XXXXXX";
static const int DEBUG_LEVEL = 0;

static char xembed_port_option[128];
static char set_working_dir_option[128];
static Display *dpy;
static DC dc;
static Atom wmatom[WMLast];
static Window root, win;
static Client **clients;
static int nclients, sel = -1, lastsel = -1;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static int cmd_append_pos;
static char winid[64];
static char **cmd;
static char *wmname = "tabbed";
static const char *geometry;

char *argv0;

/* configuration, allows nested code to access above variables */
#include "config.h"

void *create_shared_memory(size_t size) {
  // Stolen from https://stackoverflow.com/a/5656561
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}
void buttonpress(const XEvent *e) {
  const XButtonPressedEvent *ev = &e->xbutton;
  int i, fc;
  Arg arg;

  if (ev->y < 0 || ev->y > bh)
    return;

  if (((fc = getfirsttab()) > 0 && ev->x < TEXTW(before)) || ev->x < 0)
    return;

  for (i = fc; i < nclients; i++) {
    if (clients[i]->tabx > ev->x) {
      switch (ev->button) {
      case Button1:
        focus(i);
        break;
      case Button2:
        focus(i);
        killclient(NULL);
        break;
      case Button4: /* FALLTHROUGH */
      case Button5:
        arg.i = ev->button == Button4 ? -1 : 1;
        rotate(&arg);
        break;
      }
      break;
    }
  }
}

void cleanup(void) {
  int i;

  for (i = 0; i < nclients; i++) {
    focus(i);
    killclient(NULL);
    XReparentWindow(dpy, clients[i]->win, root, 0, 0);
    unmanage(i);
  }
  free(clients);
  clients = NULL;

  XFreePixmap(dpy, dc.drawable);
  XFreeGC(dpy, dc.gc);
  XDestroyWindow(dpy, win);
  XSync(dpy, False);
  free(cmd);
  int *shared_running = &shared_memory->running;
  *shared_running = False;
  // Wait for all child to quit before unmapping the shared memory
  dprintf(log_file, "[debug-tabbed] waiting for childs to terminate...\n");
  pid_t wpid;
  while ((wpid = wait(NULL)) != -1) {
    dprintf(log_file, "[debug-tabbed] CHILD `%d` exitted\n", (int)wpid);
  }
  dprintf(log_file, "[debug-tabbed] All childs terminated, we can unmap the "
                    "shared memory !\n");

  if ((void *)shared_memory != MAP_FAILED) {
    if (munmap((void *)shared_memory, SHARED_MEMORY_SIZE) == -1) {
      dprintf(log_file,
              "[error-tabbed] can't free shared memory, munmap(%ld) == -1\n",
              SHARED_MEMORY_SIZE);
    }
  }
  unlink(TABBED_LOG_FILE);
  close(log_file);
}

void clientmessage(const XEvent *e) {
  const XClientMessageEvent *ev = &e->xclient;

  if (ev->message_type == wmatom[WMProtocols] &&
      ev->data.l[0] == wmatom[WMDelete]) {
    if (nclients > 1 && killclientsfirst) {
      killclient(0);
      return;
    }
    running = False;
  }
}

void configurenotify(const XEvent *e) {
  const XConfigureEvent *ev = &e->xconfigure;

  if (ev->window == win && (ev->width != ww || ev->height != wh)) {
    ww = ev->width;
    wh = ev->height;
    XFreePixmap(dpy, dc.drawable);
    dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
    if (sel > -1)
      resize(sel, ww, wh - bh);
    XSync(dpy, False);
  }
}

void configurerequest(const XEvent *e) {
  const XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;
  int c;

  if ((c = getclient(ev->window)) > -1) {
    wc.x = 0;
    wc.y = bh;
    wc.width = ww;
    wc.height = wh - bh;
    wc.border_width = 0;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, clients[c]->win, ev->value_mask, &wc);
  }
}

void createnotify(const XEvent *e) {
  const XCreateWindowEvent *ev = &e->xcreatewindow;

  if (ev->window != win && getclient(ev->window) < 0)
    manage(ev->window);
}

void destroynotify(const XEvent *e) {
  const XDestroyWindowEvent *ev = &e->xdestroywindow;
  int c;

  if ((c = getclient(ev->window)) > -1)
    unmanage(c);
}

void die(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

void drawbar(void) {
  XftColor *col;
  int c, cc, fc, width;
  char *name = NULL;

  if (nclients == 0) {
    dc.x = 0;
    dc.w = ww;
    unsigned long nitems;
    SafeXFetchName(dpy, win, &name, &nitems);
    drawtext(name ? name : "", dc.norm, nitems);
    XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, ww, bh, 0, 0);
    XSync(dpy, False);

    return;
  }

  width = ww;
  cc = ww / tabwidth;
  if (nclients > cc)
    cc = (ww - TEXTW(before) - TEXTW(after)) / tabwidth;

  if ((fc = getfirsttab()) + cc < nclients) {
    dc.w = TEXTW(after);
    dc.x = width - dc.w;
    drawtext(after, dc.sel, sizeof(after));
    width -= dc.w;
  }
  dc.x = 0;

  if (fc > 0) {
    dc.w = TEXTW(before);
    drawtext(before, dc.sel, sizeof(before));
    dc.x += dc.w;
    width -= dc.w;
  }

  cc = MIN(cc, nclients);
  for (c = fc; c < fc + cc; c++) {
    dc.w = width / cc;
    if (c == sel) {
      col = dc.sel;
      dc.w += width % cc;
    } else {
      col = clients[c]->urgent ? dc.urg : dc.norm;
    }
    const size_t sizeof_client_name = sizeof(((Client *)0)->name);
    drawtext(clients[c]->name, col, sizeof_client_name);
    dc.x += dc.w;
    clients[c]->tabx = dc.x;
  }
  XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, ww, bh, 0, 0);
  XSync(dpy, False);
}

void drawtext(const char *text, XftColor col[ColLast], size_t size) {
  int x, y, h, len, olen;
  char buf[256];
  XftDraw *d;
  XRectangle r = {dc.x, dc.y, dc.w, dc.h};

  XSetForeground(dpy, dc.gc, col[ColBG].pixel);
  XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
  if (!text)
    return;

  olen = strnlen(text, size);
  h = dc.font.ascent + dc.font.descent;
  y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
  x = dc.x + (h / 2);

  /* shorten text if necessary */
  for (len = MIN(olen, sizeof(buf)); len && textnw(text, len) > dc.w - h; len--)
    ;

  if (!len)
    return;

  memcpy(buf, text, len);
  if (len < olen) {
    int i, j;
    for (i = len, j = strnlen(titletrim, sizeof(titletrim)); j && i;
         buf[--i] = titletrim[--j])
      ;
  }

  d = XftDrawCreate(dpy, dc.drawable, DefaultVisual(dpy, screen),
                    DefaultColormap(dpy, screen));
  XftDrawStringUtf8(d, &col[ColFG], dc.font.xfont, x, y, (XftChar8 *)buf, len);
  XftDrawDestroy(d);
}

void *ecalloc(size_t n, size_t size) {
  void *p;

  if (!(p = calloc(n, size)))
    die("%s: cannot calloc\n", argv0);
  return p;
}

void *erealloc(void *o, size_t size) {
  void *p;

  if (!(p = realloc(o, size)))
    die("%s: cannot realloc\n", argv0);
  return p;
}

void expose(const XEvent *e) {
  const XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && win == ev->window)
    drawbar();
}

void focus(int c) {
  char buf[BUFSIZ] = "tabbed-" VERSION " ::";
  XWMHints *wmh;

  /* If c, sel and clients are -1, raise tabbed-win itself */
  if (nclients == 0) {
    cmd[cmd_append_pos] = NULL;
    size_t i, n;
    for (i = 0, n = strnlen(buf, BUFSIZ); cmd[i] && n < sizeof(buf); i++)
      n += snprintf(&buf[n], sizeof(buf) - n, " %s", cmd[i]);

    xsettitle(win, buf);
    XRaiseWindow(dpy, win);

    return;
  }

  if (c < 0 || c >= nclients)
    return;

  resize(c, ww, wh - bh);
  XRaiseWindow(dpy, clients[c]->win);
  XSetInputFocus(dpy, clients[c]->win, RevertToParent, CurrentTime);
  sendxembed(c, XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
  sendxembed(c, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
  xsettitle(win, clients[c]->name);

  if (sel != c) {
    lastsel = sel;
    sel = c;
  }

  if (clients[c]->urgent && (wmh = XGetWMHints(dpy, clients[c]->win))) {
    wmh->flags &= ~XUrgencyHint;
    XSetWMHints(dpy, clients[c]->win, wmh);
    clients[c]->urgent = False;
    XFree(wmh);
  }

  drawbar();
  XSync(dpy, False);

  int *askforshellpwd = &shared_memory->askforshellpwd;
  unsigned long *current_focused_window =
      &shared_memory->current_focused_window;
  *current_focused_window = clients[c]->win;
  *askforshellpwd = True;
  dprintf(log_file,
          "[log-tabbed] New current focused window ID : `%ld`, askforshellpwd "
          ": `%s`\n",
          *current_focused_window, (*askforshellpwd) ? "True" : "False");
}

void focusin(const XEvent *e) {
  const XFocusChangeEvent *ev = &e->xfocus;
  int dummy;
  Window focused;

  if (ev->mode != NotifyUngrab) {
    XGetInputFocus(dpy, &focused, &dummy);
    if (focused == win)
      focus(sel);
  }
}

void focusonce(const Arg *arg) { nextfocus = True; }

void focusurgent(const Arg *arg) {
  int c;

  if (sel < 0)
    return;

  for (c = (sel + 1) % nclients; c != sel; c = (c + 1) % nclients) {
    if (clients[c]->urgent) {
      focus(c);
      return;
    }
  }
}

void fullscreen(const Arg *arg) {
  XEvent e;

  e.type = ClientMessage;
  e.xclient.window = win;
  e.xclient.message_type = wmatom[WMState];
  e.xclient.format = 32;
  e.xclient.data.l[0] = 2;
  e.xclient.data.l[1] = wmatom[WMFullscreen];
  e.xclient.data.l[2] = 0;
  XSendEvent(dpy, root, False, SubstructureNotifyMask, &e);
}

char *getatom(int a) {
  static char buf[BUFSIZ];
  Atom adummy;
  int idummy;
  unsigned long ldummy;
  unsigned char *p = NULL;

  XGetWindowProperty(dpy, win, wmatom[a], 0L, BUFSIZ, False, XA_STRING, &adummy,
                     &idummy, &ldummy, &ldummy, &p);
  if (p) {
    buf[0] = '\0';
    strlcat(buf, (char *)p, LENGTH(buf) - 1);
  } else {
    buf[0] = '\0';
  }
  XFree(p);

  return buf;
}

int getclient(Window w) {
  int i;

  for (i = 0; i < nclients; i++) {
    if (clients[i]->win == w)
      return i;
  }

  return -1;
}

XftColor getcolor(const char *colstr) {
  XftColor color;

  if (!XftColorAllocName(dpy, DefaultVisual(dpy, screen),
                         DefaultColormap(dpy, screen), colstr, &color))
    die("%s: cannot allocate color '%s'\n", argv0, colstr);

  return color;
}

int getfirsttab(void) {
  int cc, ret;

  if (sel < 0)
    return 0;

  cc = ww / tabwidth;
  if (nclients > cc)
    cc = (ww - TEXTW(before) - TEXTW(after)) / tabwidth;

  ret = sel - cc / 2 + (cc + 1) % 2;
  return ret < 0 ? 0 : ret + cc > nclients ? MAX(0, nclients - cc) : ret;
}

Bool gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return False;

  text[0] = '\0';
  XGetTextProperty(dpy, w, &name, atom);
  if (!name.nitems)
    return False;

  if (name.encoding == XA_STRING) {
    // Paranoïa
    text[0] = '\0';
    strlcat(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    // Paranoïa again
    text[0] = '\0';
    strlcat(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);

  return True;
}

void initfont(const char *fontstr) {
  if (!(dc.font.xfont = XftFontOpenName(dpy, screen, fontstr)) &&
      !(dc.font.xfont = XftFontOpenName(dpy, screen, "fixed")))
    die("error, cannot load font: '%s'\n", fontstr);

  dc.font.ascent = dc.font.xfont->ascent;
  dc.font.descent = dc.font.xfont->descent;
  dc.font.height = dc.font.ascent + dc.font.descent;
}

Bool isprotodel(int c) {
  int n;
  Atom *protocols;
  Bool ret = False;

  if (XGetWMProtocols(dpy, clients[c]->win, &protocols, &n)) {
    int i;
    for (i = 0; !ret && i < n; i++) {
      if (protocols[i] == wmatom[WMDelete])
        ret = True;
    }
    XFree(protocols);
  }

  return ret;
}

void keypress(const XEvent *e) {
  const XKeyEvent *ev = &e->xkey;
  unsigned int i;
  KeySym keysym;

  keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0, 0);
  for (i = 0; i < LENGTH(keys); i++) {
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
  }
}

void killclient(const Arg *arg) {
  XEvent ev;

  if (sel < 0)
    return;

  if (isprotodel(sel) && !clients[sel]->closed) {
    ev.type = ClientMessage;
    ev.xclient.window = clients[sel]->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wmatom[WMDelete];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, clients[sel]->win, False, NoEventMask, &ev);
    clients[sel]->closed = True;
  } else {
    XKillClient(dpy, clients[sel]->win);
  }
}

void manage(Window w) {
  updatenumlockmask();
  {
    int i, j, nextpos;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    KeyCode code;
    Client *c;
    XEvent e;

    XWithdrawWindow(dpy, w, 0);
    XReparentWindow(dpy, w, win, 0, bh);
    XSelectInput(dpy, w,
                 PropertyChangeMask | StructureNotifyMask | EnterWindowMask);
    XSync(dpy, False);

    for (i = 0; i < LENGTH(keys); i++) {
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym))) {
        for (j = 0; j < LENGTH(modifiers); j++) {
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], w, True,
                   GrabModeAsync, GrabModeAsync);
        }
      }
    }

    c = ecalloc(1, sizeof *c);
    c->win = w;

    nclients++;
    clients = erealloc(clients, sizeof(Client *) * nclients);

    if (npisrelative) {
      nextpos = sel + newposition;
    } else {
      if (newposition < 0)
        nextpos = nclients - newposition;
      else
        nextpos = newposition;
    }
    if (nextpos >= nclients)
      nextpos = nclients - 1;
    if (nextpos < 0)
      nextpos = 0;

    if (nclients > 1 && nextpos < nclients - 1)
      memmove(&clients[nextpos + 1], &clients[nextpos],
              sizeof(Client *) * (nclients - nextpos - 1));

    clients[nextpos] = c;
    updatetitle(nextpos);

    XLowerWindow(dpy, w);
    XMapWindow(dpy, w);

    e.xclient.window = w;
    e.xclient.type = ClientMessage;
    e.xclient.message_type = wmatom[XEmbed];
    e.xclient.format = 32;
    e.xclient.data.l[0] = CurrentTime;
    e.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = win;
    e.xclient.data.l[4] = 0;
    XSendEvent(dpy, root, False, NoEventMask, &e);

    XSync(dpy, False);

    /* Adjust sel before focus does set it to lastsel. */
    if (sel >= nextpos)
      sel++;
    focus(nextfocus ? nextpos : sel < 0 ? 0 : sel);
    nextfocus = foreground;
  }
}

void maprequest(const XEvent *e) {
  const XMapRequestEvent *ev = &e->xmaprequest;

  if (getclient(ev->window) < 0)
    manage(ev->window);
}

void move(const Arg *arg) {
  if (arg->i >= 0 && arg->i < nclients)
    focus(arg->i);
}

void movetab(const Arg *arg) {
  int c;
  Client *new;

  if (sel < 0)
    return;

  c = (sel + arg->i) % nclients;
  if (c < 0)
    c += nclients;

  if (c == sel)
    return;

  new = clients[sel];
  if (sel < c)
    memmove(&clients[sel], &clients[sel + 1], sizeof(Client *) * (c - sel));
  else
    memmove(&clients[c + 1], &clients[c], sizeof(Client *) * (sel - c));
  clients[c] = new;
  sel = c;

  drawbar();
}

void propertynotify(const XEvent *e) {
  const XPropertyEvent *ev = &e->xproperty;
  XWMHints *wmh;
  int c;
  char *selection = NULL;
  Arg arg;

  if (ev->state == PropertyNewValue && ev->atom == wmatom[WMSelectTab]) {
    selection = getatom(WMSelectTab);
    if (!strncmp(selection, "0x", 2)) {
      arg.i = getclient(strtoul(selection, NULL, 0));
      move(&arg);
    } else {
      cmd[cmd_append_pos] = selection;
      arg.v = cmd;
      spawn(&arg);
    }
  } else if (ev->state == PropertyNewValue && ev->atom == XA_WM_HINTS &&
             (c = getclient(ev->window)) > -1 &&
             (wmh = XGetWMHints(dpy, clients[c]->win))) {
    if (wmh->flags & XUrgencyHint) {
      XFree(wmh);
      wmh = XGetWMHints(dpy, win);
      if (c != sel) {
        if (urgentswitch && wmh && !(wmh->flags & XUrgencyHint)) {
          /* only switch, if tabbed was focused
           * since last urgency hint if WMHints
           * could not be received,
           * default to no switch */
          focus(c);
        } else {
          /* if no switch should be performed,
           * mark tab as urgent */
          clients[c]->urgent = True;
          drawbar();
        }
      }
      if (wmh && !(wmh->flags & XUrgencyHint)) {
        /* update tabbed urgency hint
         * if not set already */
        wmh->flags |= XUrgencyHint;
        XSetWMHints(dpy, win, wmh);
      }
    }
    XFree(wmh);
  } else if (ev->state != PropertyDelete && ev->atom == XA_WM_NAME &&
             (c = getclient(ev->window)) > -1) {
    updatetitle(c);
  }
}

void resize(int c, int w, int h) {
  XConfigureEvent ce;
  XWindowChanges wc;

  ce.x = 0;
  ce.y = bh;
  ce.width = wc.width = w;
  ce.height = wc.height = h;
  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = clients[c]->win;
  ce.window = clients[c]->win;
  ce.above = None;
  ce.override_redirect = False;
  ce.border_width = 0;

  XConfigureWindow(dpy, clients[c]->win, CWWidth | CWHeight, &wc);
  XSendEvent(dpy, clients[c]->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void rotate(const Arg *arg) {
  int nsel;

  if (sel < 0) {
    fprintf(stderr, "[error-tabbed] invalid sel `%d`\n", sel);
    return;
  }

  if (arg->i == 0) {
    if (lastsel > -1) {
      focus(lastsel);
    }
  } else if (sel > -1) {
    /* Rotating in an arg->i step around the clients. */
    nsel = sel + arg->i;
    while (nsel >= nclients)
      nsel -= nclients;
    while (nsel < 0)
      nsel += nclients;
    focus(nsel);
  }
}

void run(void) {
  XEvent ev;

  /* main event loop */
  XSync(dpy, False);
  drawbar();
  if (doinitspawn == True) {
    spawn(NULL);
  }

  while (running) {
    XNextEvent(dpy, &ev);
    if (handler[ev.type])
      (handler[ev.type])(&ev); /* call handler */
  }
}

void sendxembed(int c, long msg, long detail, long d1, long d2) {
  XEvent e = {0};

  e.xclient.window = clients[c]->win;
  e.xclient.type = ClientMessage;
  e.xclient.message_type = wmatom[XEmbed];
  e.xclient.format = 32;
  e.xclient.data.l[0] = CurrentTime;
  e.xclient.data.l[1] = msg;
  e.xclient.data.l[2] = detail;
  e.xclient.data.l[3] = d1;
  e.xclient.data.l[4] = d2;
  XSendEvent(dpy, clients[c]->win, False, NoEventMask, &e);
}

void setcmd(int argc, char *argv[], int replace) {
  int i;
  // Can possibly added 5 arguments (worst case scenario) :
  // Winid if no replace
  // xembed port option
  // xembed port value
  // working dir
  // working dir value
  cmd = ecalloc(argc + 6, sizeof(*cmd));
  if (argc == 0)
    return;
  for (i = 0; i < argc; i++)
    cmd[i] = argv[i];
  cmd[replace > 0 ? replace : argc] = winid;
  cmd_append_pos = argc + !replace;
  cmd[cmd_append_pos] = cmd[cmd_append_pos + 1] = NULL;
}

void insertcmd(char *command, size_t cmd_size, size_t pos) {
  printcmds(False);
  int commands_count = 0;
  while (cmd[commands_count++] != NULL)
    ;
  --commands_count;
  if (commands_count > 0) {
    char **copy_cmd = ecalloc(commands_count + 2, sizeof(*cmd));
    int i = 0;
    for (i = 0; i < commands_count; ++i) {
      if (i >= pos) {
        copy_cmd[i + 1] = cmd[i];
      } else {
        copy_cmd[i] = cmd[i];
      }
    }
    copy_cmd[pos] = command;
    free(cmd);
    cmd = copy_cmd;
    ++cmd_append_pos;
  }
  cmd[commands_count + 1] = NULL;
  printcmds(True);
}

void printcmds(int debug) {
  char *command = cmd[0];
  int i = 0;
  while (command != NULL) {
    if (debug) {
      dprintf(log_file, "[debug-tabbed] cmd[%d] : `%s`\n", i, command);
    } else {
      dprintf(log_file, "cmd[%d] : `%s`\n", i, command);
    }
    ++i;
    command = cmd[i];
  }
}

void setup(void) {
  int tx, ty, tw, th, isfixed;
  XWMHints *wmh;
  XClassHint class_hint;
  XSizeHints *size_hint;

  /* clean up any zombies immediately */
  sigchld(0);

  /* init screen */
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  initfont(font);
  bh = dc.h = dc.font.height + 2;

  /* init atoms */
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  wmatom[WMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMSelectTab] = XInternAtom(dpy, "_TABBED_SELECT_TAB", False);
  wmatom[WMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  wmatom[XEmbed] = XInternAtom(dpy, "_XEMBED", False);

  /* init appearance */
  wx = 0;
  wy = 0;
  ww = 800;
  wh = 600;
  isfixed = 0;

  if (geometry) {
    tx = ty = tw = th = 0;
    int bitm =
        XParseGeometry(geometry, &tx, &ty, (unsigned *)&tw, (unsigned *)&th);
    if (bitm & XValue)
      wx = tx;
    if (bitm & YValue)
      wy = ty;
    if (bitm & WidthValue)
      ww = tw;
    if (bitm & HeightValue)
      wh = th;
    if (bitm & XNegative && wx == 0)
      wx = -1;
    if (bitm & YNegative && wy == 0)
      wy = -1;
    if (bitm & (HeightValue | WidthValue))
      isfixed = 1;

    int dw = DisplayWidth(dpy, screen);
    int dh = DisplayHeight(dpy, screen);
    if (wx < 0)
      wx = dw + wx - ww - 1;
    if (wy < 0)
      wy = dh + wy - wh - 1;
  }

  dc.norm[ColBG] = getcolor(normbgcolor);
  dc.norm[ColFG] = getcolor(normfgcolor);
  dc.sel[ColBG] = getcolor(selbgcolor);
  dc.sel[ColFG] = getcolor(selfgcolor);
  dc.urg[ColBG] = getcolor(urgbgcolor);
  dc.urg[ColFG] = getcolor(urgfgcolor);
  dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
  dc.gc = XCreateGC(dpy, root, 0, 0);

  win = XCreateSimpleWindow(dpy, root, wx, wy, ww, wh, 0, dc.norm[ColFG].pixel,
                            dc.norm[ColBG].pixel);
  XMapRaised(dpy, win);
  XSelectInput(dpy, win,
               SubstructureNotifyMask | FocusChangeMask | ButtonPressMask |
                   ExposureMask | KeyPressMask | PropertyChangeMask |
                   StructureNotifyMask | SubstructureRedirectMask);
  xerrorxlib = XSetErrorHandler(xerror);

  class_hint.res_name = wmname;
  class_hint.res_class = "tabbed";
  XSetClassHint(dpy, win, &class_hint);

  size_hint = XAllocSizeHints();
  if (!isfixed) {
    size_hint->flags = PSize;
    size_hint->height = wh;
    size_hint->width = ww;
  } else {
    size_hint->flags = PMaxSize | PMinSize;
    size_hint->min_width = size_hint->max_width = ww;
    size_hint->min_height = size_hint->max_height = wh;
  }
  wmh = XAllocWMHints();
  XSetWMProperties(dpy, win, NULL, NULL, NULL, 0, size_hint, wmh, NULL);
  XFree(size_hint);
  XFree(wmh);

  XSetWMProtocols(dpy, win, &wmatom[WMDelete], 1);

  snprintf(winid, sizeof(winid), "%lu", win);
  setenv("XEMBED", winid, 1);

  nextfocus = foreground;
  focus(-1);
}

void sigchld(int unused) {
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("%s: cannot install SIGCHLD handler", argv0);

  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
}

void spawn(const Arg *arg) {
  // Set working dir here...
  if (strnlen(xembed_port_option, sizeof(xembed_port_option)) == 0 ||
      strnlen(set_working_dir_option, sizeof(set_working_dir_option)) == 0) {
    spawn_no_xembed_port(arg);
    return;
  } else if (fork() > 0) {
    if (dpy)
      close(ConnectionNumber(dpy));

    setsid();
    int is_socket_server = 0;
    if (arg && arg->v) {
      execvp(((char **)arg->v)[0], (char **)arg->v);
      fprintf(stderr, "%s: execvp %s", argv0, ((char **)arg->v)[0]);
    } else {
      // Setting up socket

      SocketListener socket_listener;
      unsigned long UNIQUE_ID = make_unique_id();
      make_socket_listener(&socket_listener, UNIQUE_ID);
      socket_listener.log_file = log_file;
      if (fork() != 0) {
        is_socket_server = 1;
        if (setup_nonblocking_listener(&socket_listener) == -1) {
          dprintf(log_file, "[error-tabbed] setup failed, exitting...\n");
        } else {
          if (DEBUG_LEVEL == 1) {
            dprintf(log_file, "[log-tabbed] starting to listen on port `%u`\n",
                    socket_listener.socket_port);
          }
          unsigned long associated_client_xid = 0;
          char shell_pwd[256];
          int associated_client_xid_asked = False,
              associated_client_xid_set = False, shell_pwd_set = False;
          int64_t last_pwd_ask_send_time = 0;
          int timeoutms = 1;

          char send_buffer[64];
          char recv_buffer[256];

          int *askforshellpwd = &shared_memory->askforshellpwd;
          unsigned long *current_focused_window =
              &shared_memory->current_focused_window;
          int was_focused = False;
          struct timespec delay_time = {.tv_sec = 0,
                                        .tv_nsec = timeoutms * 1000000};
          while (shared_memory->running) {
            // timeout regulation system
            // Only enabled once connection is setted up thus
            // the shell_pwd_set test
            if (associated_client_xid_set) {
              /* dprintf(log_file, "[debug-tabbed] Running `%lu`\n", */
              /* 		associated_client_xid); */
            }
            if (associated_client_xid_set && shell_pwd_set) {
              if (*current_focused_window == associated_client_xid) {
                if (!was_focused) {
                  timeoutms = 10;
                  was_focused = True;
                  /* dprintf(log_file, "[log-tabbed] won focus, asking client to
                   * enter turbo mode\n"); */
                  send_buffer[0] = '\0';
                  strlcat(send_buffer, "turbo", 63);
                  socket_send(&socket_listener, send_buffer, 63);
                }
              } else if (was_focused) {
                was_focused = False;
                /* dprintf(log_file, "[log-tabbed] lost focus, asking client to
                 * enter sleep mode\n"); */
                send_buffer[0] = '\0';
                strlcat(send_buffer, "sleep", 63);
                socket_send(&socket_listener, send_buffer, 63);
              }
            }
            if (!associated_client_xid_asked) {
              if (DEBUG_LEVEL == 1) {
                dprintf(log_file, "[log-tabbed] asking for client XID?\n");
              }
              send_buffer[0] = '\0';
              strlcat(send_buffer, "XID?", 63);
              if (socket_send(&socket_listener, send_buffer, 63) != -1) {
                associated_client_xid_asked = True;
              }
            }
            if (*askforshellpwd) {
              /* dprintf(log_file, "[log-tabbed]
               * current:%lu,myid:%lu,diff:%ld\n", */
              /* 		*current_focused_window, associated_client_xid,
               * get_posix_time()-last_pwd_ask_send_time); */
              if (associated_client_xid_set) {
                if (*current_focused_window == associated_client_xid) {
                  if (shell_pwd_set) {
                    // shared text string of size 256
                    char *shared_shellpwd = shared_memory->shell_pwd;
                    int *shellpwd_written = &shared_memory->shellpwd_written;
                    shared_shellpwd[0] = '\0';
                    strlcat(shared_shellpwd, shell_pwd, 255);
                    shared_shellpwd[strnlen(shell_pwd, sizeof(shell_pwd))] =
                        '\0';
                    *shellpwd_written = 1;

                    if (DEBUG_LEVEL == 1) {
                      dprintf(
                          log_file,
                          "[log-tabbed] client `%lu` wrote shell pwd `%s`\n",
                          associated_client_xid, shell_pwd);
                    }
                  }
                  int64_t time = get_posix_time();
                  if (time == -1) {
                    dprintf(log_file, "[error-tabbed] can't get time...\n");
                  } else if (time - last_pwd_ask_send_time > 500) {
                    last_pwd_ask_send_time = time;
                    if (DEBUG_LEVEL == 1) {
                      dprintf(log_file,
                              "[log-tabbed] asking for client shell PWD?\n");
                    }
                    send_buffer[0] = '\0';
                    strlcat(send_buffer, "PWD?", 63);
                    socket_send(&socket_listener, send_buffer, 63);
                    char *shared_shellpwd = shared_memory->shell_pwd;
                    if (DEBUG_LEVEL == 1) {
                      dprintf(log_file, "last shared_shellpwd : `%s`\n",
                              shared_shellpwd);
                    }
                  }
                }
              } else if (!associated_client_xid_asked) {
                if (DEBUG_LEVEL == 1) {
                  dprintf(log_file, "[log-tabbed] asking for client XID?\n");
                }
                send_buffer[0] = '\0';
                strlcat(send_buffer, "XID?", 63);
                if (socket_send(&socket_listener, send_buffer, 63) != -1) {
                  associated_client_xid_asked = True;
                }
              }
            }
            int ret = loop_listen_nonblocking(&socket_listener, recv_buffer,
                                              255, timeoutms);
            if (ret == -1) {
              dprintf(log_file, "[error-tabbed] socket loop error, exitting\n");
              break;
            } else if (ret == 1) {
              dprintf(
                  log_file,
                  "[log-tabbed] no more fd open, communication ended, XD !\n");
              break;
            } else if (ret == 2) {
              // We received something !
              if (strncmp(recv_buffer, "XID:", 4) == 0) {
                char XID_buf[32];
                XID_buf[0] = '\0';
                strlcat(XID_buf, recv_buffer + 4, 31 - 4);
                unsigned long XID = strtoul(XID_buf, NULL, 10);
                if (XID == ULONG_MAX) {
                  dprintf(
                      log_file,
                      "[error-tabbed] invalid XID : strtoul(%s, 10) == ERROR\n",
                      XID_buf);
                } else {
                  associated_client_xid = XID;
                  associated_client_xid_set = True;
                  dprintf(log_file,
                          "[log-tabbed] received client's XID : `%lu`\n",
                          associated_client_xid);
                }
              } else if (strncmp(recv_buffer, "PWD:", 4) == 0) {
                /* dprintf(log_file, "[log-tabbed] client `%lu` wrote shell pwd
                 * `%s`\n", */
                /* 		associated_client_xid, recv_buffer); */
                shell_pwd[0] = '\0';
                strlcat(shell_pwd, recv_buffer + 4, 255 - 4);
                if (!shell_pwd_set) {
                  dprintf(log_file, "[log-tabbed] Up and running in `%lu`ms\n",
                          get_posix_time() - shared_memory->debug_time);
                }
                shell_pwd_set = True;
                /* dprintf(log_file, "[log-tabbed] received client's PWD :
                 * `%s`\n", shell_pwd); */
              } else {
                dprintf(log_file,
                        "[error-tabbed] received unknown command : `%s`\n",
                        recv_buffer);
              }
            }
            /* else if (ret == 0) {
                    // No error and not finished, continue looping
            }*/
            delay_time.tv_nsec = timeoutms * 1000000;
            nanosleep(&delay_time, NULL);
          }
          dprintf(
              log_file,
              "[log-tabbed] client communication on port `%u` terminated.\n",
              socket_listener.socket_port);
          cleanup_socket(&socket_listener);
          dprintf(log_file, "[log-tabbed] socket cleaned up.\n");
        }
      } else {
        // Child
        // Should wait until lock port file exists
        // Wait until lock file is filled with port, 1ms delay between
        // iterations
        unsigned long socket_port =
            wait_for_socket_port_lock(&socket_listener, 1);
        if (socket_port == -1) {
          dprintf(log_file, "[error-tabbed] failed to wait for socket port "
                            "lock, exitting...\n");
        } else {
          char *shared_shellpwd = shared_memory->shell_pwd;
          int *shellpwd_written = &shared_memory->shellpwd_written;
          // Can't use mmap share memory outside of this processus ?
          char buffer_shellpwd[256];

          if (strnlen(xembed_port_option, sizeof(xembed_port_option)) != 0 &&
              strnlen(set_working_dir_option, sizeof(set_working_dir_option))) {
            char buf[23];
            snprintf(buf, 22, "%lu", socket_port);
            cmd[cmd_append_pos] = xembed_port_option;
            cmd[cmd_append_pos + 1] = buf;
            cmd[cmd_append_pos + 2] = NULL;
            if (*shellpwd_written) {
              buffer_shellpwd[0] = '\0';
              strlcat(buffer_shellpwd, shared_shellpwd,
                      sizeof(buffer_shellpwd) - 1);
              insertcmd(buffer_shellpwd,
                        strnlen(buffer_shellpwd, sizeof(buffer_shellpwd)), 1);
              insertcmd(set_working_dir_option,
                        strnlen(set_working_dir_option,
                                sizeof(set_working_dir_option)),
                        1);
              // Bad position
              /* cmd[cmd_append_pos+2] = "--working-directory"; */
              /* cmd[cmd_append_pos+3] = buffer_shellpwd; */
            }
          } else {
            if (*shellpwd_written) {
              buffer_shellpwd[0] = '\0';
              strlcat(buffer_shellpwd, shared_shellpwd, 255);
              /* cmd[cmd_append_pos] = "--working-directory"; */
              /* cmd[cmd_append_pos+1] = buffer_shellpwd; */
            } else {
              cmd[cmd_append_pos] = NULL;
            }
          }
          shared_memory->debug_time = get_posix_time();
          /* dprintf(log_file, "[debug-tabbed] cmd : `%s`\n",cmd[0]); */
          /* printcmds(True); */
          execvp(cmd[0], cmd);
          dprintf(log_file, "%s: execvp %s", argv0, cmd[0]);
        }
      }
    }
    if (!is_socket_server) {
      dprintf(log_file, "Failed\n");
      perror(" failed");
      exit(0);
    }
  }
}
void spawn_no_xembed_port(const Arg *arg) {
  // Set working dir here...
  if (fork() > 0) {
    if (dpy)
      close(ConnectionNumber(dpy));

    setsid();
    if (arg && arg->v) {
      execvp(((char **)arg->v)[0], (char **)arg->v);
      fprintf(stderr, "%s: execvp %s", argv0, ((char **)arg->v)[0]);
    } else {
      cmd[cmd_append_pos] = NULL;
      execvp(cmd[0], cmd);
      dprintf(log_file, "%s: execvp %s", argv0, cmd[0]);
    }
  }
}

int textnw(const char *text, unsigned int len) {
  XGlyphInfo ext;
  XftTextExtentsUtf8(dpy, dc.font.xfont, (XftChar8 *)text, len, &ext);
  return ext.xOff;
}

void toggle(const Arg *arg) { *(Bool *)arg->v = !*(Bool *)arg->v; }

void unmanage(int c) {
  if (c < 0 || c >= nclients) {
    drawbar();
    XSync(dpy, False);
    return;
  }

  if (!nclients)
    return;

  if (c == 0) {
    /* First client. */
    nclients--;
    free(clients[0]);
    memmove(&clients[0], &clients[1], sizeof(Client *) * nclients);
  } else if (c == nclients - 1) {
    /* Last client. */
    nclients--;
    free(clients[c]);
    clients = erealloc(clients, sizeof(Client *) * nclients);
  } else {
    /* Somewhere inbetween. */
    free(clients[c]);
    memmove(&clients[c], &clients[c + 1],
            sizeof(Client *) * (nclients - (c + 1)));
    nclients--;
  }

  if (nclients <= 0) {
    lastsel = sel = -1;

    if (closelastclient)
      running = False;
    else if (fillagain && running) {
      dprintf(log_file, "[log-tabbed] unmanage spawn\n");
      spawn(NULL);
    }
  } else {
    if (lastsel >= nclients)
      lastsel = nclients - 1;
    else if (lastsel > c)
      lastsel--;

    if (c == sel && lastsel >= 0) {
      focus(lastsel);
    } else {
      if (sel > c)
        sel--;
      if (sel >= nclients)
        sel = nclients - 1;

      focus(sel);
    }
  }

  drawbar();
  XSync(dpy, False);
}

void unmapnotify(const XEvent *e) {
  const XUnmapEvent *ev = &e->xunmap;
  int c;

  if ((c = getclient(ev->window)) > -1)
    unmanage(c);
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++) {
    for (j = 0; j < modmap->max_keypermod; j++) {
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
    }
  }
  XFreeModifiermap(modmap);
}

void updatetitle(int c) {
  if (!gettextprop(clients[c]->win, wmatom[WMName], clients[c]->name,
                   sizeof(clients[c]->name))) {
    gettextprop(clients[c]->win, XA_WM_NAME, clients[c]->name,
                sizeof(clients[c]->name));
  }
  if (sel == c) {
    xsettitle(win, clients[c]->name);
  }
  drawbar();
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;

  dprintf(log_file, "%s: fatal error: request code=%d, error code=%d\n", argv0,
          ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

void xsettitle(Window w, const char *str) {
  XTextProperty xtp;

  if (XmbTextListToTextProperty(dpy, (char **)&str, 1, XCompoundTextStyle,
                                &xtp) == Success) {
    XSetTextProperty(dpy, w, &xtp, wmatom[WMName]);
    XSetTextProperty(dpy, w, &xtp, XA_WM_NAME);
    XFree(xtp.value);
  }
}

void usage(void) {
  die("usage: %s [-dfksv] [-g geometry] [-n name] [-p [s+/-]pos]\n"
      "       [-r narg] [-o color] [-O color] [-t color] [-T color]\n"
      "       [-u color] [-U color] [ -x option ] [ -w option ] command...\n",
      argv0);
}

int main(int argc, char *argv[]) {
  Bool detach = False;
  int replace = 0;
  char *pstr;
  char *wstr;
  char *xstr;
  int xembed_port_option_set = 0, set_working_dir_option_set = 0;
  xembed_port_option[0] = '\0';
  set_working_dir_option[0] = '\0';

  ARGBEGIN {
  case 'c':
    closelastclient = True;
    fillagain = False;
    break;
  case 'd':
    detach = True;
    break;
  case 'f':
    fillagain = True;
    break;
  case 'g':
    geometry = EARGF(usage());
    break;
  case 'k':
    killclientsfirst = True;
    break;
  case 'n':
    wmname = EARGF(usage());
    break;
  case 'O':
    normfgcolor = EARGF(usage());
    break;
  case 'o':
    normbgcolor = EARGF(usage());
    break;
  case 'p':
    pstr = EARGF(usage());
    if (pstr[0] == 's') {
      npisrelative = True;
      newposition = atoi(&pstr[1]);
    } else {
      newposition = atoi(pstr);
    }
    break;
  case 'r':
    replace = atoi(EARGF(usage()));
    break;
  case 's':
    doinitspawn = False;
    break;
  case 'T':
    selfgcolor = EARGF(usage());
    break;
  case 't':
    selbgcolor = EARGF(usage());
    break;
  case 'U':
    urgfgcolor = EARGF(usage());
    break;
  case 'u':
    urgbgcolor = EARGF(usage());
    break;
  case 'v':
    die("tabbed-" VERSION ", © 2009-2016 tabbed engineers, "
        "see LICENSE for details.\n");
    break;
  case 'w':
    wstr = EARGF(usage());
    set_working_dir_option[0] = '\0';
    strlcat(set_working_dir_option, wstr, 127);
    set_working_dir_option_set = 1;
    break;
  case 'x':
    xstr = EARGF(usage());
    xembed_port_option[0] = '\0';
    strlcat(xembed_port_option, xstr, 127);
    xembed_port_option_set = 1;
    break;
  default:
    usage();
    break;
  }
  ARGEND;

  if (!xembed_port_option_set) {
    char *port_option = getenv("TABBED_XEMBED_PORT_OPTION");
    if (port_option != NULL) {
      xembed_port_option[0] = '\0';
      strlcat(xembed_port_option, port_option, 127);
      dprintf(log_file, "[log-tabbed]  TABBED_XEMBED_PORT_OPTION = `%s`\n",
              xembed_port_option);
    } else {
      xembed_port_option[0] = '\0';
    }
  }
  if (!set_working_dir_option_set) {
    char *port_option = getenv("TABBED_WORKING_DIR_OPTION");
    if (port_option != NULL) {
      set_working_dir_option[0] = '\0';
      strlcat(set_working_dir_option, port_option, 127);
      dprintf(log_file, "[log-tabbed]  TABBED_WORKING_DIR_OPTION = `%s`\n",
              set_working_dir_option);
    } else {
      set_working_dir_option[0] = '\0';
    }
  }

  if (argc < 1) {
    doinitspawn = False;
    fillagain = False;
  }
  log_file = mkstemp(TABBED_LOG_FILE);
  if (log_file == -1) {
    dprintf(log_file,
            "[error-tabbed] can't make log file : mkstemp(%s) == -1\n",
            TABBED_LOG_FILE);
    return -1;
  }

  // Initialize shared memory for
  shared_memory = (SharedMemory *)create_shared_memory(SHARED_MEMORY_SIZE);
  if ((void *)shared_memory == MAP_FAILED) {
    dprintf(log_file,
            "[error-tabbed] couldn't create shared memory, mmap(%ld) == ERROR",
            SHARED_MEMORY_SIZE);
    cleanup();
    return -1;
  }
  memset(shared_memory, 0, SHARED_MEMORY_SIZE);
  int *shared_running = &shared_memory->running;
  *shared_running = True;
  // setting NULL-terminated string
  char *shared_memory_shellpwd = shared_memory->shell_pwd;
  *shared_memory_shellpwd = '\0';

  setcmd(argc, argv, replace);

  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    dprintf(log_file, "%s: no locale support\n", argv0);
  if (!(dpy = XOpenDisplay(NULL)))
    die("%s: cannot open display\n", argv0);

  setup();
  printf("0x%lx\n", win);
  fflush(NULL);

  if (detach) {
    if (fork() == 0) {
      fclose(stdout);
    } else {
      if (dpy)
        close(ConnectionNumber(dpy));
      return EXIT_SUCCESS;
    }
  }

  run();
  cleanup();
  XCloseDisplay(dpy);
  dprintf(log_file, "[debug-tabbed] finished!\n");

  return EXIT_SUCCESS;
}
