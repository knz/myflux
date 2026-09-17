/*
 * ctmset: set the DRM/RandR "CTM" (color transformation matrix) output
 * property directly, since xrandr's own --set only handles scalar
 * properties and CTM is a 9-element S31.32 sign-magnitude blob.
 *
 * Matrix layout matches struct drm_color_ctm (uapi/drm/drm_mode.h):
 *   |R|   |m0 m1 m2|   |R|
 *   |G| = |m3 m4 m5| x |G|
 *   |B|   |m6 m7 m8|   |B|
 *
 * Usage: ctmset <output-name> <m0> <m1> ... <m8>
 * Example (identity / reset):
 *   ctmset eDP 1 0 0  0 1 0  0 0 1
 * Example (Rec.709 grayscale):
 *   ctmset eDP 0.2126 0.7152 0.0722  0.2126 0.7152 0.0722  0.2126 0.7152 0.0722
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

/* Encode a double as S31.32 sign-magnitude, per drm_mode.h. */
static uint64_t encode_s31_32(double v) {
    uint64_t sign = 0;
    if (v < 0) {
        sign = 1ULL << 63;
        v = -v;
    }
    uint64_t magnitude = (uint64_t)(v * 4294967296.0 /* 2^32 */ + 0.5);
    return sign | magnitude;
}

static RROutput find_output(Display *dpy, XRRScreenResources *res, const char *name) {
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (info && strcmp(info->name, name) == 0) {
            RROutput out = res->outputs[i];
            XRRFreeOutputInfo(info);
            return out;
        }
        if (info) XRRFreeOutputInfo(info);
    }
    return None;
}

int main(int argc, char **argv) {
    if (argc != 11) {
        fprintf(stderr, "usage: %s <output-name> <m0> <m1> ... <m8>\n", argv[0]);
        fprintf(stderr, "  e.g. identity:  %s eDP 1 0 0 0 1 0 0 0 1\n", argv[0]);
        return 2;
    }

    const char *output_name = argv[1];
    double m[9];
    for (int i = 0; i < 9; i++) {
        char *end;
        m[i] = strtod(argv[2 + i], &end);
        if (end == argv[2 + i] || *end != '\0') {
            fprintf(stderr, "error: '%s' is not a valid number\n", argv[2 + i]);
            return 2;
        }
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "error: could not open X display (is DISPLAY set correctly?)\n");
        return 1;
    }

    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        fprintf(stderr, "error: XRRGetScreenResourcesCurrent failed\n");
        return 1;
    }

    RROutput output = find_output(dpy, res, output_name);
    if (output == None) {
        fprintf(stderr, "error: no such output '%s'\n", output_name);
        XRRFreeScreenResources(res);
        return 1;
    }

    Atom ctm_atom = XInternAtom(dpy, "CTM", False);

    uint64_t ctm[9];
    for (int i = 0; i < 9; i++) ctm[i] = encode_s31_32(m[i]);

    /* Split each 64-bit value into two 32-bit words (native byte order),
     * since XRRChangeOutputProperty format 32 only carries CARD32 units.
     * Xlib's format-32 convention requires the client array element type
     * to be `long`, even though only the low 32 bits go on the wire. */
    long data[18];
    for (int i = 0; i < 9; i++) {
        uint32_t lo = (uint32_t)(ctm[i] & 0xFFFFFFFFu);
        uint32_t hi = (uint32_t)(ctm[i] >> 32);
        data[2 * i] = (long)lo;
        data[2 * i + 1] = (long)hi;
    }

    XRRChangeOutputProperty(dpy, output, ctm_atom, XA_INTEGER, 32,
                             PropModeReplace, (unsigned char *)data, 18);
    XFlush(dpy);
    XSync(dpy, False);

    fprintf(stderr, "CTM set on %s:\n  |%.6f %.6f %.6f|\n  |%.6f %.6f %.6f|\n  |%.6f %.6f %.6f|\n",
            output_name, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);

    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    return 0;
}
