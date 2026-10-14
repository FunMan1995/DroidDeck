/*
 * DroidDeck presentation option (b), client half (brief 003b §4). Part of the DroidDeck build of
 * the Vortek client, LGPL-2.1 (tools/vortek/LICENSE).
 *
 * The renderer copies each presented image into a shared memory fd it hands over at
 * vkCreateSwapchainKHR. Here the frame is put into the Xlib window (gamescope's Xwayland) with
 * MIT-SHM when available, else XPutImage. libX11/libXext are loaded at runtime so the ICD has no
 * link-time X dependency.
 */
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include "droiddeck_shims.h"

typedef struct { unsigned long shmseg; int shmid; char* shmaddr; Bool readOnly; } DDXShmSegmentInfo;

static struct {
    bool tried, ok, shm;
    Status (*GetGeometry)(Display*, Drawable, Window*, int*, int*, unsigned*, unsigned*, unsigned*, unsigned*);
    GC (*CreateGC)(Display*, Drawable, unsigned long, XGCValues*);
    int (*FreeGC)(Display*, GC);
    XImage* (*CreateImage)(Display*, Visual*, unsigned, int, int, char*, unsigned, unsigned, int, int);
    int (*PutImage)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned, unsigned);
    int (*Sync)(Display*, Bool);
    int (*Flush)(Display*);
    int (*Free)(void*);
    Status (*GetWindowAttributes)(Display*, Window, XWindowAttributes*);
    Bool (*ShmQueryExtension)(Display*);
    Bool (*ShmAttach)(Display*, DDXShmSegmentInfo*);
    Bool (*ShmDetach)(Display*, DDXShmSegmentInfo*);
    XImage* (*ShmCreateImage)(Display*, Visual*, unsigned, int, char*, DDXShmSegmentInfo*, unsigned, unsigned);
    Bool (*ShmPutImage)(Display*, Drawable, GC, XImage*, int, int, int, int, unsigned, unsigned, Bool);
} X;

static pthread_mutex_t xLoadLock = PTHREAD_MUTEX_INITIALIZER;

static bool loadX(void) {
    pthread_mutex_lock(&xLoadLock);
    if (!X.tried) {
        X.tried = true;
        void* x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
        if (x11) {
            X.GetGeometry = dlsym(x11, "XGetGeometry");
            X.CreateGC = dlsym(x11, "XCreateGC");
            X.FreeGC = dlsym(x11, "XFreeGC");
            X.CreateImage = dlsym(x11, "XCreateImage");
            X.PutImage = dlsym(x11, "XPutImage");
            X.Sync = dlsym(x11, "XSync");
            X.Flush = dlsym(x11, "XFlush");
            X.Free = dlsym(x11, "XFree");
            X.GetWindowAttributes = dlsym(x11, "XGetWindowAttributes");
            X.ok = X.GetGeometry && X.CreateGC && X.FreeGC && X.CreateImage && X.PutImage && X.Sync && X.Flush && X.Free && X.GetWindowAttributes;
        }
        void* xext = X.ok ? dlopen("libXext.so.6", RTLD_NOW) : NULL;
        if (xext) {
            X.ShmQueryExtension = dlsym(xext, "XShmQueryExtension");
            X.ShmAttach = dlsym(xext, "XShmAttach");
            X.ShmDetach = dlsym(xext, "XShmDetach");
            X.ShmCreateImage = dlsym(xext, "XShmCreateImage");
            X.ShmPutImage = dlsym(xext, "XShmPutImage");
            X.shm = X.ShmQueryExtension && X.ShmAttach && X.ShmDetach && X.ShmCreateImage && X.ShmPutImage;
        }
        if (!X.ok) fprintf(stderr, "vortek: libX11 not available; frames are not shown\n");
    }
    pthread_mutex_unlock(&xLoadLock);
    return X.ok;
}

bool dd_windowExtent(Display* dpy, Window window, VkExtent2D* extent) {
    if (!dpy || !window || !loadX()) return false;
    Window root; int x, y; unsigned w, h, border, depth;
    if (!X.GetGeometry(dpy, window, &root, &x, &y, &w, &h, &border, &depth)) return false;
    extent->width = w;
    extent->height = h;
    return true;
}

struct DDPresent {
    Display* dpy;
    Window window;
    int fd;
    void* map;
    size_t size;
    GC gc;
    XImage* image;
    DDXShmSegmentInfo seg;
    bool useShm;
    char* pixels; /* XPutImage fallback */
};

DDPresent* dd_presentCreate(Display* dpy, Window window, int fd) {
    DDPresent* p = calloc(1, sizeof(DDPresent));
    p->dpy = dpy;
    p->window = window;
    p->fd = fd;
    p->seg.shmid = -1;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(DDShmFrameHeader)) goto error;
    p->size = st.st_size;
    p->map = mmap(NULL, p->size, PROT_READ, MAP_SHARED, fd, 0);
    if (p->map == MAP_FAILED) { p->map = NULL; goto error; }
    if (!dpy || !loadX()) return p;

    const DDShmFrameHeader* h = p->map;
    XWindowAttributes attrs;
    if (!X.GetWindowAttributes(dpy, window, &attrs)) return p;
    p->gc = X.CreateGC(dpy, window, 0, NULL);
    size_t bytes = (size_t)h->stride * h->height;
    if (X.shm && X.ShmQueryExtension(dpy)) {
        p->image = X.ShmCreateImage(dpy, attrs.visual, attrs.depth, ZPixmap, NULL, &p->seg, h->width, h->height);
        if (p->image) {
            p->seg.shmid = shmget(IPC_PRIVATE, (size_t)p->image->bytes_per_line * p->image->height, IPC_CREAT | 0600);
            p->seg.shmaddr = p->seg.shmid >= 0 ? shmat(p->seg.shmid, NULL, 0) : (char*)-1;
            if (p->seg.shmaddr != (char*)-1 && X.ShmAttach(dpy, &p->seg)) {
                X.Sync(dpy, False);
                shmctl(p->seg.shmid, IPC_RMID, NULL);
                p->image->data = p->seg.shmaddr;
                p->useShm = true;
                return p;
            }
            if (p->seg.shmaddr != (char*)-1) shmdt(p->seg.shmaddr);
            if (p->seg.shmid >= 0) shmctl(p->seg.shmid, IPC_RMID, NULL);
            p->seg.shmid = -1;
            p->image->data = NULL;
            X.Free(p->image);
            p->image = NULL;
        }
    }
    p->pixels = malloc(bytes);
    p->image = X.CreateImage(dpy, attrs.visual, attrs.depth, ZPixmap, 0, p->pixels, h->width, h->height, 32, h->stride);
    return p;

error:
    dd_presentDestroy(p);
    return NULL;
}

/* Returns VK_ERROR_OUT_OF_DATE_KHR when the window no longer matches the frame size. */
VkResult dd_presentFrame(DDPresent* p) {
    if (!p || !p->map) return VK_ERROR_SURFACE_LOST_KHR;
    const DDShmFrameHeader* h = p->map;
    if (!p->image) return VK_SUCCESS;
    const uint32_t* src = (const uint32_t*)((const char*)p->map + sizeof(DDShmFrameHeader));
    char* dst = p->image->data;
    for (uint32_t y = 0; y < h->height; y++) {
        uint32_t* row = (uint32_t*)(dst + (size_t)y * p->image->bytes_per_line);
        const uint32_t* in = src + (size_t)y * (h->stride / 4);
        if (h->rgba) {
            for (uint32_t x = 0; x < h->width; x++) {
                uint32_t v = in[x];
                row[x] = (v & 0xff00ff00u) | ((v & 0xffu) << 16) | ((v >> 16) & 0xffu);
            }
        } else memcpy(row, in, (size_t)h->width * 4);
    }
    if (p->useShm) X.ShmPutImage(p->dpy, p->window, p->gc, p->image, 0, 0, 0, 0, h->width, h->height, False);
    else X.PutImage(p->dpy, p->window, p->gc, p->image, 0, 0, 0, 0, h->width, h->height);
    X.Flush(p->dpy);

    VkExtent2D now;
    if (dd_windowExtent(p->dpy, p->window, &now) && (now.width != h->width || now.height != h->height)) return VK_ERROR_OUT_OF_DATE_KHR;
    return VK_SUCCESS;
}

void dd_presentDestroy(DDPresent* p) {
    if (!p) return;
    if (p->useShm) {
        X.ShmDetach(p->dpy, &p->seg);
        X.Sync(p->dpy, False);
        shmdt(p->seg.shmaddr);
        p->image->data = NULL;
    } else if (p->image) {
        p->image->data = NULL;
    }
    if (p->image) X.Free(p->image);
    if (p->gc) X.FreeGC(p->dpy, p->gc);
    free(p->pixels);
    if (p->map) munmap(p->map, p->size);
    if (p->fd >= 0) close(p->fd);
    free(p);
}
