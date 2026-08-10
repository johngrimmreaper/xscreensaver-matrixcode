#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    Display *display;
    Window root;
    Window root_return;
    Window parent_return;
    Window *children = NULL;
    unsigned int child_count = 0U;
    unsigned int i;
    long width;
    long height;
    char *end = NULL;
    int found = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s WIDTH HEIGHT\n", argv[0]);
        return 2;
    }
    width = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || width <= 0L) return 2;
    end = NULL;
    height = strtol(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end != '\0' || height <= 0L) return 2;

    display = XOpenDisplay(NULL);
    if (display == NULL) return 3;
    root = DefaultRootWindow(display);
    if (!XQueryTree(display, root, &root_return, &parent_return,
                    &children, &child_count)) {
        XCloseDisplay(display);
        return 3;
    }
    for (i = 0U; i < child_count; i++) {
        char *name = NULL;
        if (XFetchName(display, children[i], &name) != 0 && name != NULL) {
            if (strcmp(name, "MatrixCode 1999 film study") == 0) {
                XResizeWindow(display, children[i],
                              (unsigned int)width, (unsigned int)height);
                XSync(display, False);
                found = 1;
                XFree(name);
                break;
            }
            XFree(name);
        }
    }
    if (children != NULL) XFree(children);
    XCloseDisplay(display);
    return found ? 0 : 4;
}
