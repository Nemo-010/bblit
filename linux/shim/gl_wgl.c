/* gl_wgl.c -- real OpenGL backend for the Linux port (stage 2).
 *
 * The 1999 game drives graphics through two Windows APIs:
 *   1. DirectDraw (display modes, fullscreen cooperative level)
 *   2. WGL (GL context creation, pixel formats, buffer swap)
 *
 * On Linux there is no DirectDraw and no WGL.  What the game actually
 * needs from each is small:
 *   - DirectDraw: agree to its mode requests (the game then does all
 *     rendering through OpenGL, never through DDraw surfaces).
 *   - WGL: a real GL context with a drawable, swap, and proc addresses.
 *
 * Display strategy: EGL surfaceless + Mesa software rendering when no
 * display server is present (headless sandbox, CI), and the same path
 * everywhere until a real X11/Wayland window backend lands.  The game's
 * HWND/HDC stay opaque cookies for game logic; the drawable behind them
 * is an EGL pbuffer.  STUB-DIVERGENCE: no visible window yet.
 */
#include "winshim.h"
#include "data_syms.h"

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>

#define EGL_NO_X11
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

/* ---- EGL state ---- */
static EGLDisplay egl_dpy = EGL_NO_DISPLAY;
static EGLSurface egl_surf = EGL_NO_SURFACE;
static EGLContext egl_ctx = EGL_NO_CONTEXT;
static EGLConfig egl_cfg = 0;
static int egl_ready = 0;

static HDC wgl_cur_dc = 0;
static HGLRC wgl_cur_ctx = 0;

/* Game's requested client size lives in the image; fall back to 640x480. */
static void game_size(int *w, int *h)
{
    /* Fixed pbuffer: the game drives the viewport itself via glViewport.
     * (DAT_00467748/4c are floats the scaler owns; reading them as the
     * drawable size gave 512x4.) */
    *w = 640;
    *h = 480;
}

static int ensure_egl(void)
{
    EGLint major, minor, ncfg;
    EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE,
    };
    EGLint pb_attribs[5];
    int w, h;

    if (egl_ready) return 1;
    egl_dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                    EGL_DEFAULT_DISPLAY, NULL);
    if (egl_dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "[egl] no surfaceless display\n");
        return 0;
    }
    if (!eglInitialize(egl_dpy, &major, &minor)) {
        fprintf(stderr, "[egl] initialize failed\n");
        return 0;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        fprintf(stderr, "[egl] bind API failed e=%#x\n", eglGetError());
        return 0;
    }
    if (!eglChooseConfig(egl_dpy, cfg_attribs, &egl_cfg, 1, &ncfg) || ncfg < 1) {
        fprintf(stderr, "[egl] no config\n");
        return 0;
    }
    game_size(&w, &h);
    pb_attribs[0] = EGL_WIDTH;  pb_attribs[1] = w;
    pb_attribs[2] = EGL_HEIGHT; pb_attribs[3] = h;
    pb_attribs[4] = EGL_NONE;
    egl_surf = eglCreatePbufferSurface(egl_dpy, egl_cfg, pb_attribs);
    if (egl_surf == EGL_NO_SURFACE) {
        fprintf(stderr, "[egl] no pbuffer e=%#x\n", eglGetError());
        return 0;
    }
    egl_ctx = eglCreateContext(egl_dpy, egl_cfg, EGL_NO_CONTEXT, NULL);
    if (egl_ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "[egl] no context e=%#x\n", eglGetError());
        return 0;
    }
    egl_ready = 1;
    fprintf(stderr, "[egl] surfaceless %dx%d ctx=%p\n", w, h, (void *)egl_ctx);
    return 1;
}

/* ---- WGL entry points (wgl* names the game resolves via GetProcAddress) ---- */
/* Context handles: the game parks the HGLRC in a 4-byte image slot,
 * so raw EGL pointers (64-bit, ASLRd past 4 GiB) would truncate.
 * Small integer handles round-trip exactly; they index this table. */
static EGLContext ctx_handles[8];
static EGLContext resolve_ctx(void *h)
{
    uintptr_t id = (uintptr_t)h;
    /* The decompile drops wgl args staged in x86 frame slots, so
     * MakeCurrent/Delete arrive with stack garbage; fall back to
     * the context global the game maintains (DAT_004b1ac0). */
    if (id == 0 || id > 7 || ctx_handles[id] == EGL_NO_CONTEXT) {
        uintptr_t g = (uintptr_t)DAT_004b1ac0;
        if (g != 0 && g < 8 && ctx_handles[g] != EGL_NO_CONTEXT)
            return ctx_handles[g];
        return egl_ctx;
    }
    return ctx_handles[id];
}
HGLRC wglCreateContext(HDC hdc)
{
    (void)hdc;
    if (!ensure_egl()) return 0;
    /* One shared context is enough: the game creates a single context
     * and makes it current for the life of the window. */
    ctx_handles[1] = egl_ctx;
    return (HGLRC)(uintptr_t)1;
}

BOOL wglMakeCurrent(HDC hdc, HGLRC ctx)
{
    EGLContext c;
    if (!ensure_egl()) return 0;
    c = resolve_ctx(ctx);
    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, c)) {
        fprintf(stderr, "[egl] makeCurrent failed e=%#x\n", eglGetError());
        return 0;
    }
    wgl_cur_dc = hdc;
    wgl_cur_ctx = ctx;
    return 1;
}

BOOL wglDeleteContext(HGLRC ctx)
{
    (void)ctx;
    /* Keep the shared context alive for the process lifetime; the game
     * deletes and recreates across mode switches, and a dead context
     * would break the next MakeCurrent. */
    wgl_cur_ctx = 0;
    return 1;
}

HGLRC wglGetCurrentContext(void) { return wgl_cur_ctx; }
HDC wglGetCurrentDC(void) { return wgl_cur_dc; }

int wglChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR *pfd)
{
    (void)hdc; (void)pfd;
    return 1;
}

int wglDescribePixelFormat(HDC hdc, int ipfd, UINT nbytes, PIXELFORMATDESCRIPTOR *pfd)
{
    (void)hdc; (void)ipfd;
    if (pfd && nbytes >= sizeof(*pfd)) {
        memset(pfd, 0, sizeof(*pfd));
        pfd->nSize = sizeof(*pfd);
        pfd->nVersion = 1;
        pfd->dwFlags = 0x25;
        pfd->cColorBits = 16;
    }
    return 1;
}

UINT wglGetPixelFormat(HDC hdc) { (void)hdc; return 1; }

BOOL wglSetPixelFormat(HDC hdc, int ipfd, const PIXELFORMATDESCRIPTOR *pfd)
{
    (void)hdc; (void)ipfd; (void)pfd;
    return 1;
}

static int wgl_swap(HDC hdc)
{
    (void)hdc;
    if (!egl_ready) return 0;
    eglSwapBuffers(egl_dpy, egl_surf);
    return 1;
}

BOOL wglSwapBuffers(HDC hdc) { return wgl_swap(hdc); }
BOOL wglSwapLayerBuffers(HDC hdc, UINT planes) { (void)planes; return wgl_swap(hdc); }

/* GDI SwapBuffers reaches the same drawable. */
BOOL SwapBuffers(HDC hdc) { return wgl_swap(hdc); }

BOOL wglShareLists(HGLRC a, HGLRC b) { (void)a; (void)b; return 1; }
BOOL wglCopyContext(HGLRC s, HGLRC d, UINT m) { (void)s; (void)d; (void)m; return 1; }
HGLRC wglCreateLayerContext(HDC hdc, int layer) { (void)layer; return wglCreateContext(hdc); }
int wglGetLayerPaletteEntries(HDC h, int s, int n, COLORREF *c) { (void)h; (void)s; (void)n; (void)c; return 0; }
int wglSetLayerPaletteEntries(HDC h, int s, int n, const COLORREF *c) { (void)h; (void)s; (void)n; (void)c; return 0; }
BOOL wglRealizeLayerPalette(HDC h, int r, BOOL r2) { (void)h; (void)r; (void)r2; return 1; }

/* Font outlines have no GLX/EGL equivalent; report success with no
 * display lists built.  STUB-DIVERGENCE: in-game text may be missing. */
BOOL wglUseFontBitmapsA(HDC h, DWORD f, DWORD c, DWORD b) { (void)h; (void)f; (void)c; (void)b; return 1; }
BOOL wglUseFontBitmapsW(HDC h, DWORD f, DWORD c, DWORD b) { (void)h; (void)f; (void)c; (void)b; return 1; }
BOOL wglUseFontOutlinesA(HDC h, DWORD f, DWORD c, DWORD b, float dx, float dy, int fm, void *gm)
{ (void)h; (void)f; (void)c; (void)b; (void)dx; (void)dy; (void)fm; (void)gm; return 1; }
BOOL wglUseFontOutlinesW(HDC h, DWORD f, DWORD c, DWORD b, float dx, float dy, int fm, void *gm)
{ (void)h; (void)f; (void)c; (void)b; (void)dx; (void)dy; (void)fm; (void)gm; return 1; }

FARPROC wglGetProcAddress(LPCSTR name)
{
    FARPROC p;
    if (!name) return 0;
    p = (FARPROC)eglGetProcAddress(name);
    if (p) return p;
    /* gl* names also resolve here on Windows; cover them via libGL. */
    p = (FARPROC)dlsym(RTLD_DEFAULT, name);
    return p;
}

/* Sub-4GB trampolines: the image GL table stores 4-byte proc addresses,
 * but system libraries (libGL under ASan, EGL) can map above 4 GiB and
 * the low half alone jumps wild (observed: torn glDepthFunc target).
 * A 14-byte jmp-qword stub below 4 GiB round-trips exactly through the
 * 4-byte slot; the side table then carries the stub address. */
#include <sys/mman.h>
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
static unsigned char *tramp_base = 0;
static unsigned tramp_used = 0;
static unsigned tramp_pool_size = 0;
static void *tramp_targets[1024];
static void *tramp_stubs[1024];
static unsigned tramp_ncached = 0;
void *gl_trampoline(void *target)
{
    unsigned char *t;
    unsigned i;
    if ((uintptr_t)target <= 0xffffffffu)
        return target;
    /* The loader re-resolves all 360 names on every ogl_init retry;
     * reuse stubs so the pool cannot exhaust (observed: 2nd sync
     * spilled raw >4GB targets into 4-byte slots -> wild jump). */
    for (i = 0; i < tramp_ncached; i++)
        if (tramp_targets[i] == target)
            return tramp_stubs[i];
    if (!tramp_base) {
        tramp_pool_size = 65536;
        tramp_base = mmap(0, tramp_pool_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        if (tramp_base == (void *)-1)
            return target;
        if ((uintptr_t)tramp_base > 0xffffffffu)
            return target;
    }
    if (tramp_used + 14 > tramp_pool_size || tramp_ncached >= 1024)
        return target;
    t = tramp_base + tramp_used;
    tramp_used += 14;
    /* ff 25 00000000: jmp qword [rip+0]; addr */
    t[0] = 0xff; t[1] = 0x25; t[2] = 0; t[3] = 0; t[4] = 0; t[5] = 0;
    __builtin_memcpy(t + 6, &target, 8);
    __builtin___clear_cache((char *)t, (char *)t + 14);
    tramp_targets[tramp_ncached] = target;
    tramp_stubs[tramp_ncached] = t;
    tramp_ncached++;
    return t;
}

/* Resolve a wgl* name to the wrapper above.  Returns 0 for non-wgl. */
FARPROC wgl_wrapper(LPCSTR name)
{
    if (!name || strncmp(name, "wgl", 3) != 0) return 0;
    if (!strcmp(name, "wglCreateContext")) return (FARPROC)wglCreateContext;
    if (!strcmp(name, "wglMakeCurrent")) return (FARPROC)wglMakeCurrent;
    if (!strcmp(name, "wglDeleteContext")) return (FARPROC)wglDeleteContext;
    if (!strcmp(name, "wglGetCurrentContext")) return (FARPROC)wglGetCurrentContext;
    if (!strcmp(name, "wglGetCurrentDC")) return (FARPROC)wglGetCurrentDC;
    if (!strcmp(name, "wglChoosePixelFormat")) return (FARPROC)wglChoosePixelFormat;
    if (!strcmp(name, "wglDescribePixelFormat")) return (FARPROC)wglDescribePixelFormat;
    if (!strcmp(name, "wglGetPixelFormat")) return (FARPROC)wglGetPixelFormat;
    if (!strcmp(name, "wglSetPixelFormat")) return (FARPROC)wglSetPixelFormat;
    if (!strcmp(name, "wglSwapBuffers")) return (FARPROC)wglSwapBuffers;
    if (!strcmp(name, "wglSwapLayerBuffers")) return (FARPROC)wglSwapLayerBuffers;
    if (!strcmp(name, "wglShareLists")) return (FARPROC)wglShareLists;
    if (!strcmp(name, "wglCopyContext")) return (FARPROC)wglCopyContext;
    if (!strcmp(name, "wglCreateLayerContext")) return (FARPROC)wglCreateLayerContext;
    if (!strcmp(name, "wglGetProcAddress")) return (FARPROC)wglGetProcAddress;
    if (!strcmp(name, "wglGetDefaultProcAddress")) return (FARPROC)wglGetProcAddress;
    if (!strcmp(name, "wglGetLayerPaletteEntries")) return (FARPROC)wglGetLayerPaletteEntries;
    if (!strcmp(name, "wglSetLayerPaletteEntries")) return (FARPROC)wglSetLayerPaletteEntries;
    if (!strcmp(name, "wglRealizeLayerPalette")) return (FARPROC)wglRealizeLayerPalette;
    if (!strcmp(name, "wglUseFontBitmapsA")) return (FARPROC)wglUseFontBitmapsA;
    if (!strcmp(name, "wglUseFontBitmapsW")) return (FARPROC)wglUseFontBitmapsW;
    if (!strcmp(name, "wglUseFontOutlinesA")) return (FARPROC)wglUseFontOutlinesA;
    if (!strcmp(name, "wglUseFontOutlinesW")) return (FARPROC)wglUseFontOutlinesW;
    return 0;
}

/* Headless GL probe: clear + readback checksum.  Returns 1 on real GL. */
int gl_probe_frame(unsigned *checksum)
{
    static const GLfloat green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    unsigned char px[16 * 16 * 4];
    unsigned sum = 0;
    int w, h, n;
    if (!ensure_egl()) return 0;
    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) return 0;
    game_size(&w, &h);
    glViewport(0, 0, w > 16 ? 16 : w, h > 16 ? 16 : h);
    glClearColor(green[0], green[1], green[2], green[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    n = (w > 16 ? 16 : w) * (h > 16 ? 16 : h) * 4;
    glReadPixels(0, 0, w > 16 ? 16 : w, h > 16 ? 16 : h,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int i = 0; i < n; i++) sum += px[i];
    if (checksum) *checksum = sum;
    return sum != 0;
}

/* ---- fake IDirectDraw ----
 * Vtable byte offsets the game calls (from objdump call sites):
 *   +0x08 Release, +0x30 GetDisplayMode, +0x50 SetCooperativeLevel,
 *   +0x54 SetDisplayMode.  All report success; the game renders through
 *   OpenGL and never touches DDraw surfaces. */
typedef int (*ddraw_fn)(void *self, ...);

static int ddraw_query(void *s, ...) { (void)s; return 0; }
static int ddraw_addref(void *s, ...) { (void)s; return 1; }
static int ddraw_release(void *s, ...) { (void)s; return 0; }
static int ddraw_ok(void *s, ...) { (void)s; return 0; }

static ddraw_fn ddraw_vtable[23];

static void *fake_dd_obj[1];

void ddraw_fake_init(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    for (int i = 0; i < 23; i++) ddraw_vtable[i] = ddraw_ok;
    ddraw_vtable[0] = ddraw_query;   /* QueryInterface */
    ddraw_vtable[1] = ddraw_addref;  /* AddRef */
    ddraw_vtable[2] = ddraw_release; /* Release (+0x08) */
    /* +0x30 GetDisplayMode, +0x50 SetCooperativeLevel,
     * +0x54 SetDisplayMode already DD_OK via ddraw_ok. */
    fake_dd_obj[0] = ddraw_vtable;
}

void *ddraw_fake_object(void)
{
    ddraw_fake_init();
    return fake_dd_obj;
}

/* GL dispatch side table: the image packs 360 proc slots at
 * 4-byte stride, but LP64 reads them as 8-byte code pointers
 * (a torn read calls garbage; observed at wglCreateContext).
 * The loader stores 4-byte values; gl_table_sync() zero-extends
 * them here, and data_syms.h redirects the called slots.` */
static const unsigned gl_table_dests[360] = {
    0x007c62d0, 0x007c63c0, 0x007c9010, 0x007c638c,
    0x007c64d8, 0x007c6368, 0x007c6264, 0x007c63c8,
    0x007c6278, 0x007c6564, 0x007c6318, 0x007c6598,
    0x007c6274, 0x007c8fec, 0x007c647c, 0x007c9018,
    0x007c648c, 0x007c64b4, 0x007c6398, 0x007c64b8,
    0x007c6390, 0x007c64c0, 0x007c635c, 0x007c64a0,
    0x007c6344, 0x007c6494, 0x007c6328, 0x007c6310,
    0x007c9484, 0x007c6330, 0x007c9110, 0x007c6324,
    0x007c90f4, 0x007c64bc, 0x007c6358, 0x007c64a8,
    0x007c634c, 0x007c64ac, 0x007c6378, 0x007c64b0,
    0x007c6364, 0x007c649c, 0x007c6440, 0x007c6320,
    0x007c90f0, 0x007c6448, 0x007c90d4, 0x007c642c,
    0x007c90b8, 0x007c657c, 0x007c65ec, 0x007c65f0,
    0x007c6580, 0x007c6290, 0x007c629c, 0x007c9030,
    0x007c9058, 0x007c62e4, 0x007c6388, 0x007c6574,
    0x007c90a8, 0x007c6628, 0x007c9098, 0x007c639c,
    0x007c8ff4, 0x007c630c, 0x007c62bc, 0x007c8fe4,
    0x007c63e0, 0x007c6360, 0x007c644c, 0x007c6588,
    0x007c632c, 0x007c6474, 0x007c62a8, 0x007c63fc,
    0x007c90a4, 0x007c6260, 0x007c9094, 0x007c62f0,
    0x007c908c, 0x007c62f4, 0x007c90a0, 0x007c62fc,
    0x007c6450, 0x007c6404, 0x007c6338, 0x007c6334,
    0x007c628c, 0x007c90c4, 0x007c63a4, 0x007c8fe0,
    0x007c6370, 0x007c8fe8, 0x007c636c, 0x007c6630,
    0x007c6444, 0x007c63d8, 0x007c6434, 0x007c9460,
    0x007c6540, 0x007c901c, 0x007c6614, 0x007c659c,
    0x007c63bc, 0x007c9000, 0x007c90b4, 0x007c6510,
    0x007c6528, 0x007c64f0, 0x007c9008, 0x007c8ffc,
    0x007c9080, 0x007c9050, 0x007c9468, 0x007c6394,
    0x007c9350, 0x007c6304, 0x007c9020, 0x007c9028,
    0x007c62b4, 0x007c62d8, 0x007c62ec, 0x007c6340,
    0x007c6294, 0x007c62b0, 0x007c63c4, 0x007c63dc,
    0x007c9070, 0x007c65bc, 0x007c9334, 0x007c932c,
    0x007c8fdc, 0x007c9330, 0x007c651c, 0x007c9320,
    0x007c6518, 0x007c6270, 0x007c6554, 0x007c6560,
    0x007c6308, 0x007c64ec, 0x007c63ac, 0x007c8ff0,
    0x007c909c, 0x007c6314, 0x007c6590, 0x007c625c,
    0x007c658c, 0x007c6268, 0x007c62dc, 0x007c6460,
    0x007c62e0, 0x007c6454, 0x007c6384, 0x007c6610,
    0x007c907c, 0x007c9090, 0x007c63e4, 0x007c63e8,
    0x007c65f4, 0x007c63cc, 0x007c6284, 0x007c6288,
    0x007c62b8, 0x007c62cc, 0x007c6350, 0x007c633c,
    0x007c6348, 0x007c6374, 0x007c63f0, 0x007c6480,
    0x007c63ec, 0x007c646c, 0x007c90ac, 0x007c90e8,
    0x007c90d8, 0x007c9328, 0x007c6414, 0x007c62a4,
    0x007c6424, 0x007c62c8, 0x007c641c, 0x007c627c,
    0x007c6428, 0x007c6298, 0x007c63f8, 0x007c6254,
    0x007c90c0, 0x007c946c, 0x007c9060, 0x007c63b0,
    0x007c64d4, 0x007c645c, 0x007c6608, 0x007c6604,
    0x007c64cc, 0x007c64c4, 0x007c6458, 0x007c626c,
    0x007c6354, 0x007c6468, 0x007c6400, 0x007c631c,
    0x007c8fd4, 0x007c6380, 0x007c6490, 0x007c6408,
    0x007c6484, 0x007c64c8, 0x007c6478, 0x007c6470,
    0x007c6594, 0x007c65e4, 0x007c65b8, 0x007c65d8,
    0x007c65b4, 0x007c652c, 0x007c65c4, 0x007c655c,
    0x007c65b0, 0x007c65c0, 0x007c65ac, 0x007c6520,
    0x007c65a8, 0x007c64f8, 0x007c65e0, 0x007c6534,
    0x007c65a0, 0x007c6508, 0x007c65cc, 0x007c64e4,
    0x007c65c8, 0x007c6570, 0x007c65dc, 0x007c8fd0,
    0x007c62c4, 0x007c63a8, 0x007c6638, 0x007c63d4,
    0x007c6624, 0x007c6430, 0x007c662c, 0x007c643c,
    0x007c8fd8, 0x007c6410, 0x007c90b0, 0x007c65e8,
    0x007c65d4, 0x007c6504, 0x007c6500, 0x007c62f8,
    0x007c64e0, 0x007c62d4, 0x007c63a0, 0x007c6488,
    0x007c62e8, 0x007c9088, 0x007c9464, 0x007c9084,
    0x007c9324, 0x007c9064, 0x007c9338, 0x007c903c,
    0x007c90e0, 0x007c9068, 0x007c933c, 0x007c905c,
    0x007c9108, 0x007c9074, 0x007c910c, 0x007c904c,
    0x007c90d0, 0x007c9078, 0x007c9118, 0x007c906c,
    0x007c90f8, 0x007c9044, 0x007c9100, 0x007c902c,
    0x007c90bc, 0x007c9048, 0x007c9104, 0x007c9040,
    0x007c90e4, 0x007c9054, 0x007c90ec, 0x007c9034,
    0x007c6300, 0x007c65a4, 0x007c6438, 0x007c9494,
    0x007c6600, 0x007c9348, 0x007c640c, 0x007c6634,
    0x007c6420, 0x007c6618, 0x007c6418, 0x007c6620,
    0x007c90c8, 0x007c90cc, 0x007c62c0, 0x007c64f4,
    0x007c62ac, 0x007c6514, 0x007c65fc, 0x007c65f8,
    0x007c6544, 0x007c6568, 0x007c9004, 0x007c656c,
    0x007c900c, 0x007c6558, 0x007c9480, 0x007c6550,
    0x007c948c, 0x007c64e8, 0x007c947c, 0x007c654c,
    0x007c9488, 0x007c6538, 0x007c9474, 0x007c6530,
    0x007c9344, 0x007c64dc, 0x007c9470, 0x007c6524,
    0x007c9478, 0x007c650c, 0x007c9498, 0x007c64fc,
    0x007c9340, 0x007c64d0, 0x007c6464, 0x007c660c,
    0x007c6584, 0x007c9114, 0x007c934c, 0x007c90dc,
    0x007c661c, 0x007c64a4, 0x007c90fc, 0x007c6258,
    0x007c63b4, 0x007c6498, 0x007c63d0, 0x007c6280,
    0x007c63f4, 0x007c63b8, 0x007c65d0, 0x007c9014,
    0x007c637c, 0x007c9038, 0x007c9490, 0x007c6548,
    0x007c8ff8, 0x007c9024, 0x007c653c, 0x007c6578,
};
void *gl_proc_side[360];
void gl_table_sync(void)
{
    for (int i = 0; i < 360; i++)
        gl_proc_side[i] = (void *)(uintptr_t)*(unsigned *)gl_table_dests[i];
    if (getenv("BBLIT_DEBUG")) {
        fprintf(stderr, "[glsync] HMODULE image=%#x macro=%p\n",
                *(unsigned *)0x9ca724ul, (void *)(uintptr_t)DAT_009ca724);
        /* spot-check the slots ogl_init calls first */
        static const unsigned want[] = {0x7c90a8,0x7c9098,0x7c632c,0x7c6274,0x7c934c,0x7c63b8};
        for (int k = 0; k < 6; k++) {
            for (int i = 0; i < 360; i++)
                if (gl_table_dests[i] == want[k])
                    fprintf(stderr, "[glsync] %#x -> %p\n", want[k], gl_proc_side[i]);
        }
    }
}

/* Callback side table: the 0x4b1a5c-0x4b1aa4 dispatch slots sit at
 * 4-byte stride but are read and written as 8-byte pointers, so
 * adjacent stores tear each other (observed: DAT_004b1a80 read back
 * doubled halves after the _1a84 store).  The side struct carries
 * exact values; seeded from the image at startup so pre-assignment
 * reads still reach the port trampolines. */
struct cb1a_side cb1a;
struct cb1a_side cb1a;
void cb1a_init(void)
{
    cb1a.f004b1a5c = (code)(uintptr_t)*(unsigned *)0x00004b1a5cul;
    cb1a.f004b1a60 = (code)(uintptr_t)*(unsigned *)0x00004b1a60ul;
    cb1a.f004b1a64 = (code)(uintptr_t)*(unsigned *)0x00004b1a64ul;
    cb1a.f004b1a68 = (code)(uintptr_t)*(unsigned *)0x00004b1a68ul;
    cb1a.f004b1a74 = (code)(uintptr_t)*(unsigned *)0x00004b1a74ul;
    cb1a.f004b1a78 = (code)(uintptr_t)*(unsigned *)0x00004b1a78ul;
    cb1a.f004b1a7c = (code)(uintptr_t)*(unsigned *)0x00004b1a7cul;
    cb1a.f004b1a80 = (code)(uintptr_t)*(unsigned *)0x00004b1a80ul;
    cb1a.p004b1a84 = (data_u32 *)(uintptr_t)*(unsigned *)0x0004b1a84ul;
    cb1a.p004b1a88 = (data_u32 *)(uintptr_t)*(unsigned *)0x0004b1a88ul;
    cb1a.f004b1a8c = (code)(uintptr_t)*(unsigned *)0x00004b1a8cul;
    cb1a.f004b1a90 = (code)(uintptr_t)*(unsigned *)0x00004b1a90ul;
    cb1a.p004b1a94 = (data_u32 *)(uintptr_t)*(unsigned *)0x0004b1a94ul;
    cb1a.p004b1a98 = (data_u32 *)(uintptr_t)*(unsigned *)0x0004b1a98ul;
    cb1a.f004b1a9c = (code)(uintptr_t)*(unsigned *)0x00004b1a9cul;
    cb1a.p004b1aa0 = (data_u32 *)(uintptr_t)*(unsigned *)0x0004b1aa0ul;
    cb1a.f004b1aa4 = (code)(uintptr_t)*(unsigned *)0x00004b1aa4ul;
}
