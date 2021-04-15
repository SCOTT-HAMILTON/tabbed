#ifndef SAFE_XFETCHNAME
#define SAFE_XFETCHNAME

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Xatom.h>
#include <X11/Xlibint.h>
#include <X11/Xos.h>
#include <stdio.h>

Status SafeXFetchName(register Display *dpy, Window w, char **name,
                      unsigned long *nitems) {
  Atom actual_type;
  int actual_format;
  unsigned long leftover;
  unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, w, XA_WM_NAME, 0L, (long)BUFSIZ, False, XA_STRING,
                         &actual_type, &actual_format, nitems, &leftover,
                         &data) != Success) {
    *name = NULL;
    return (0);
  }
  if ((actual_type == XA_STRING) && (actual_format == 8)) {

    /* The data returned by XGetWindowProperty is guaranteed to
    contain one extra byte that is null terminated to make retrieveing
    string properties easy. */

    *name = (char *)data;
    return (1);
  }
  Xfree(data);
  *name = NULL;
  return (0);
}

#endif // SAFE_XFETCHNAME
