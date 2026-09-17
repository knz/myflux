/* Read the CTM property back as raw words, to distinguish "the X server
 * stored the wrong bits" from "xrandr prints them wrong". */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

int main(int argc, char **argv) {
    Display *dpy = XOpenDisplay(NULL);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, DefaultRootWindow(dpy));
    RROutput out = 0;
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *inf = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (inf && !strcmp(inf->name, argv[1])) out = res->outputs[i];
        if (inf) XRRFreeOutputInfo(inf);
    }
    Atom ctm = XInternAtom(dpy, "CTM", False);
    Atom type; int fmt; unsigned long n, bytes; unsigned char *data = NULL;
    XRRGetOutputProperty(dpy, out, ctm, 0, 100, False, False, AnyPropertyType,
                         &type, &fmt, &n, &bytes, &data);
    printf("format=%d nitems=%lu\n", fmt, n);
    long *w = (long *)data;
    for (unsigned long i = 0; i < n && i < 6; i += 2) {
        uint32_t lo = (uint32_t)w[i], hi = (uint32_t)w[i+1];
        uint64_t v = ((uint64_t)hi << 32) | lo;
        double mag = (double)(v & 0x7FFFFFFFFFFFFFFFULL) / 4294967296.0;
        printf("  m%lu: raw_long[lo]=0x%016lX raw_long[hi]=0x%016lX"
               " -> lo=0x%08X hi=0x%08X  value=%s%.6f\n",
               i/2, (unsigned long)w[i], (unsigned long)w[i+1], lo, hi,
               (v >> 63) ? "-" : "", mag);
    }
    return 0;
}
