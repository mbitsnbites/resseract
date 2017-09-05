// main.cpp: initialisation & main loop

#include "engine/engine.h"

#include <cstdio>

//-------------------------------------------------------------------------------------------------
// Imported functions and variables.
// TODO(m): These should be in some header file somewhere.
//-------------------------------------------------------------------------------------------------

extern void clear_command();
extern void clear_console();
extern void clear_models();
extern void clear_sound();

extern int fullscreen;
extern int sound;
extern int soundchans;
extern int soundfreq;
extern int soundbufferlen;
extern int mesa_swap_bug;
extern int curvsync;
extern int vsync;
extern int vsynctear;

//-------------------------------------------------------------------------------------------------
// Global (exported) variables.
//-------------------------------------------------------------------------------------------------

int screenw = 0;
int screenh = 0;
SDL_Window *screen = NULL;
int curtime = 0;
int lastmillis = 1;
int elapsedtime = 0;
int totalmillis = 1;
dynent *player = NULL;
int initing = NOT_INITING;
float loadprogress = 0;
bool grabinput = false;
bool minimized = false;
int curvsync = -1;
bool inbetweenframes = false;
bool renderedframe = true;

#ifdef _WIN32
// Force Optimus setups to use the NVIDIA GPU.
extern "C"
{
#ifdef __GNUC__
__attribute__((dllexport))
#else
__declspec(dllexport)
#endif
DWORD NvOptimusEnablement = 1;

#ifdef __GNUC__
__attribute__((dllexport))
#else
__declspec(dllexport)
#endif
DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace
{
    //-------------------------------------------------------------------------------------------------
    // Local constants.
    //-------------------------------------------------------------------------------------------------

    const int SCR_MINW = 320;
    const int SCR_MINH = 200;
    const int SCR_MAXW = 10000;
    const int SCR_MAXH = 10000;
    const int SCR_DEFAULTH = 768;
#ifdef _DEBUG
    const int DEFAULT_FULLSCREEN = 0;
#else
    const int DEFAULT_FULLSCREEN = 1;
#endif
    const int MAXFPSHISTORY = 60;

    //-------------------------------------------------------------------------------------------------
    // Local variables.
    //-------------------------------------------------------------------------------------------------

    SDL_GLContext s_glcontext = NULL;
    string s_backgroundcaption = "";
    Texture *s_backgroundmapshot = NULL;
    string s_backgroundmapname = "";
    char *s_backgroundmapinfo = NULL;

    int s_fpspos = 0;
    int s_fpshistory[MAXFPSHISTORY];
    int s_clockrealbase = 0;
    int s_clockvirtbase = 0;

    bool s_shouldgrab = false;
    bool s_canrelativemouse = true;
    bool s_relativemouse = false;
    int s_keyrepeatmask = 0;
    int s_textinputmask = 0;
    Uint32 s_textinputtime = 0;

    bool s_initwindowpos = false;

    int s_curgamma = 100;
    vector<SDL_Event> s_events;

}  // namespace

//-------------------------------------------------------------------------------------------------
// Game variables.
//-------------------------------------------------------------------------------------------------

static void clockreset();
static void setfullscreen(bool enable);
static void setgamma(int val);
static void restorevsync();

VAR(desktopw, 1, 0, 0);
VAR(desktoph, 1, 0, 0);
VARFN(screenw, scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARFN(screenh, scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));
VAR(menumute, 0, 1, 1);
VAR(menufps, 0, 60, 1000);
VARP(maxfps, 0, 125, 1000);
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());
VAR(numcpus, 1, 1, 16);
VAR(progressbackground, 0, 0, 1);
VARNP(relativemouse, userelativemouse, 0, 1, 1);
VAR(textinputfilter, 0, 5, 1000);
VARF(fullscreen, 0, DEFAULT_FULLSCREEN, 1, setfullscreen(fullscreen != 0));

VARFP(gamma, 30, 100, 300, {
    if (initing || gamma == s_curgamma)
    {
        return;
    }
    s_curgamma = gamma;
    setgamma(s_curgamma);
});

VARFP(vsync, 0, 0, 1, restorevsync());
VARFP(vsynctear, 0, 0, 1, {
    if (vsync)
    {
        restorevsync();
    }
});
VAR(dbgmodes, 0, 0, 1);

//-------------------------------------------------------------------------------------------------
// Local functions.
//-------------------------------------------------------------------------------------------------

static void getbackgroundres(int &w, int &h)
{
    float wk = 1, hk = 1;
    if (w < 1024)
    {
        wk = 1024.0f / w;
    }
    if (h < 768)
    {
        hk = 768.0f / h;
    }
    wk = hk = max(wk, hk);
    w = int(ceil(w * wk));
    h = int(ceil(h * hk));
}

static void setfullscreen(bool enable)
{
    if (!screen)
    {
        return;
    }
    // initwarning(enable ? "fullscreen" : "windowed");
    SDL_SetWindowFullscreen(screen, enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!enable)
    {
        SDL_SetWindowSize(screen, scr_w, scr_h);
        if (s_initwindowpos)
        {
            int winx = SDL_WINDOWPOS_CENTERED, winy = SDL_WINDOWPOS_CENTERED;
            SDL_SetWindowPosition(screen, winx, winy);
            s_initwindowpos = false;
        }
    }
}

static void setgamma(int val)
{
    if (screen && SDL_SetWindowBrightness(screen, val / 100.0f) < 0)
    {
        conoutf(CON_ERROR, "Could not set gamma: %s", SDL_GetError());
    }
}

static void restorevsync()
{
    if (initing || !s_glcontext)
    {
        return;
    }
    if (!SDL_GL_SetSwapInterval(vsync ? (vsynctear ? -1 : 1) : 0))
    {
        curvsync = vsync;
    }
}

static bool filterevent(const SDL_Event &event)
{
    switch (event.type)
    {
        case SDL_MOUSEMOTION:
            if (grabinput && !s_relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
            {
                if (event.motion.x == screenw / 2 && event.motion.y == screenh / 2)
                    return false;  // ignore any motion events generated by SDL_WarpMouse
#ifdef __APPLE__
                if (event.motion.y == 0)
                    return false;  // let mac users drag windows via the title bar
#endif
            }
            break;
    }
    return true;
}

static inline bool pollevent(SDL_Event &event)
{
    while (SDL_PollEvent(&event))
    {
        if (filterevent(event))
        {
            return true;
        }
    }
    return false;
}

static void ignoremousemotion()
{
    SDL_Event e;
    SDL_PumpEvents();
    while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION))
    {
    }
}

static void resetmousemotion()
{
    if (grabinput && !s_relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        SDL_WarpMouseInWindow(screen, screenw / 2, screenh / 2);
    }
}

static void checkmousemotion(int &dx, int &dy)
{
    loopv(s_events)
    {
        SDL_Event &event = s_events[i];
        if (event.type != SDL_MOUSEMOTION)
        {
            if (i > 0)
            {
                s_events.remove(0, i);
            }
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
    s_events.setsize(0);
    SDL_Event event;
    while (pollevent(event))
    {
        if (event.type != SDL_MOUSEMOTION)
        {
            s_events.add(event);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
}

static bool findarg(int argc, char **argv, const char *str)
{
    for (int i = 1; i < argc; i++)
    {
        if (strstr(argv[i], str) == argv[i])
        {
            return true;
        }
    }
    return false;
}

static void clockreset()
{
    s_clockrealbase = static_cast<int>(SDL_GetTicks());
    s_clockvirtbase = totalmillis;
}

static void cleargamma()
{
    if (s_curgamma != 100 && screen)
    {
        SDL_SetWindowBrightness(screen, 1.0f);
    }
}

static void cleanup()
{
    recorder::stop();
    cleanupserver();
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if (screen)
    {
        SDL_SetWindowGrab(screen, SDL_FALSE);
    }
    cleargamma();
    freeocta(worldroot);
    UI::cleanup();
    clear_command();
    clear_console();
    clear_models();
    clear_sound();
    closelogfile();
#ifdef __APPLE__
    if (screen)
    {
        SDL_SetWindowFullscreen(screen, 0);
    }
#endif
    SDL_Quit();
}

static void writeinitcfg()
{
    stream *f = openutf8file("config/init.cfg", "w");
    if (!f)
    {
        return;
    }
    f->printf("// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    f->printf("fullscreen %d\n", fullscreen);
    f->printf("screenw %d\n", scr_w);
    f->printf("screenh %d\n", scr_h);
    f->printf("sound %d\n", sound);
    f->printf("soundchans %d\n", soundchans);
    f->printf("soundfreq %d\n", soundfreq);
    f->printf("soundbufferlen %d\n", soundbufferlen);
    delete f;
}

static void quit()  // normal exit
{
    writeinitcfg();
    writeservercfg();
    abortconnect();
    disconnect();
    localdisconnect();
    writecfg();
    cleanup();
    exit(EXIT_SUCCESS);
}

static void bgquad(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x, y);
    gle::attribf(tx, ty);
    gle::attribf(x + w, y);
    gle::attribf(tx + tw, ty);
    gle::attribf(x, y + h);
    gle::attribf(tx, ty + th);
    gle::attribf(x + w, y + h);
    gle::attribf(tx + tw, ty + th);
    gle::end();
}

static void renderbackgroundview(int w,
                                 int h,
                                 const char *caption,
                                 Texture *mapshot,
                                 const char *mapname,
                                 const char *mapinfo)
{
    static int lastupdate = -1, lastw = -1, lasth = -1;
    static float backgroundu = 0, backgroundv = 0;
    if ((renderedframe && !mainmenu && lastupdate != lastmillis) || lastw != w || lasth != h)
    {
        lastupdate = lastmillis;
        lastw = w;
        lasth = h;

        backgroundu = rndscale(1);
        backgroundv = rndscale(1);
    }
    else if (lastupdate != lastmillis)
    {
        lastupdate = lastmillis;
    }

    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    gle::defvertex(2);
    gle::deftexcoord0();

    settexture("media/interface/background.png", 0);
    float bu = w * 0.67f / 256.0f, bv = h * 0.67f / 256.0f;
    bgquad(0, 0, w, h, backgroundu, backgroundv, bu, bv);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    settexture("media/interface/shadow.png", 3);
    bgquad(0, 0, w, h);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    const float lh = 0.2f * static_cast<float>(min(w, h));
    const float lw = lh * (1500.0f / 320.0f);
    const float lx = 0.5f * (static_cast<float>(w) - lw);
    const float ly = 0.1f * lh;
    const int texmax = (maxtexsize ? min(maxtexsize, hwtexsize) : hwtexsize);
    const char* logo;
    if ((texmax >= 1500) && (lw >= 1150.0f))
    {
        logo = "<premul>media/interface/logo_1500.png";
    }
    else if ((texmax >= 1024) && (lw >= 600.0f))
    {
        logo = "<premul>media/interface/logo_1024.png";
    }
    else
    {
        logo = "<premul>media/interface/logo_512.png";
    }
    settexture(logo, 3);
    bgquad(lx, ly, lw, lh);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (caption)
    {
        int tw = text_width(caption);
        float tsz = 0.04f * min(w, h) / FONTH, tx = 0.5f * (w - tw * tsz),
              ty = h - 0.075f * 1.5f * min(w, h) - FONTH * tsz;
        pushhudtranslate(tx, ty, tsz);
        draw_text(caption, 0, 0);
        pophudmatrix();
    }
    if (mapshot || mapname)
    {
        float infowidth = 14 * FONTH, sz = 0.35f * min(w, h), msz = (0.85f * min(w, h) - sz) / (infowidth + FONTH),
              x = 0.5f * w, y = ly + lh - sz / 15, mx = 0, my = 0, mw = 0, mh = 0;
        if (mapinfo)
        {
            text_boundsf(mapinfo, mw, mh, infowidth);
            x -= 0.5f * mw * msz;
            if (mapshot && mapshot != notexture)
            {
                x -= 0.5f * FONTH * msz;
                mx = sz + FONTH * msz;
            }
        }
        if (mapshot && mapshot != notexture)
        {
            x -= 0.5f * sz;
            resethudshader();
            glBindTexture(GL_TEXTURE_2D, mapshot->id);
            bgquad(x, y, sz, sz);
        }
        if (mapname)
        {
            float tw = text_widthf(mapname), tsz = sz / (8 * FONTH), tx = max(0.5f * (mw * msz - tw * tsz), 0.0f);
            pushhudtranslate(x + mx + tx, y, tsz);
            draw_text(mapname, 0, 0);
            pophudmatrix();
            my = 1.5f * FONTH * tsz;
        }
        if (mapinfo)
        {
            pushhudtranslate(x + mx, y + my, msz);
            draw_text(mapinfo, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, -1, infowidth);
            pophudmatrix();
        }
    }

    glDisable(GL_BLEND);
}

static void setbackgroundinfo(const char *caption = NULL,
                              Texture *mapshot = NULL,
                              const char *mapname = NULL,
                              const char *mapinfo = NULL)
{
    renderedframe = false;
    copystring(s_backgroundcaption, caption ? caption : "");
    s_backgroundmapshot = mapshot;
    copystring(s_backgroundmapname, mapname ? mapname : "");
    if (mapinfo != s_backgroundmapinfo)
    {
        DELETEA(s_backgroundmapinfo);
        if (mapinfo)
        {
            s_backgroundmapinfo = newstring(mapinfo);
        }
    }
}

static void restorebackground(int w, int h, bool force = false)
{
    if (renderedframe)
    {
        if (!force)
        {
            return;
        }
        setbackgroundinfo();
    }
    renderbackgroundview(w,
                         h,
                         s_backgroundcaption[0] ? s_backgroundcaption : NULL,
                         s_backgroundmapshot,
                         s_backgroundmapname[0] ? s_backgroundmapname : NULL,
                         s_backgroundmapinfo);
}

// Note: Also used during loading.
static void renderprogressview(int w, int h, float bar, const char *text)
{
    hudmatrix.ortho(0, w, h, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    gle::defvertex(2);
    gle::deftexcoord0();

    const float fh = 0.060f * min(w, h);
    const float fw = fh * 15;
    const float fx = renderedframe ? w - fw - fh / 4 : 0.5f * (w - fw);
    const float fy = renderedframe ? fh / 4 : h - fh * 1.5f;
    settexture("media/interface/loading_frame.png", 3);
    bgquad(fx, fy, fw, fh);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float bw = fw * (512 - 2 * 8) / 512.0f;
    const float bh = fh * 20 / 32.0f;
    const float bx = fx + fw * 8 / 512.0f;
    const float by = fy + fh * 6 / 32.0f;
    const float su1 = 0 / 32.0f;
    const float su2 = 8 / 32.0f;
    const float sw = fw * 8 / 512.0f;
    const float eu1 = 24 / 32.0f;
    const float eu2 = 32 / 32.0f;
    const float ew = fw * 8 / 512.0f;
    const float mw = bw - sw - ew;
    const float ex = bx + sw + max(mw * bar, fw * 8 / 512.0f);
    if (bar > 0)
    {
        settexture("media/interface/loading_bar.png", 3);
        bgquad(bx, by, sw, bh, su1, 0, su2 - su1, 1);
        bgquad(bx + sw, by, ex - (bx + sw), bh, su2, 0, eu1 - su2, 1);
        bgquad(ex, by, ew, bh, eu1, 0, eu2 - eu1, 1);
    }

    if (text)
    {
        const int tw = text_width(text);
        float tsz = bh * 0.6f / FONTH;
        if (tw * tsz > mw)
        {
            tsz = mw / tw;
        }

        pushhudtranslate(bx + sw, by + (bh - FONTH * tsz) / 2, tsz);
        draw_text(text, 0, 0);
        pophudmatrix();
    }

    glDisable(GL_BLEND);
}

static void inputgrab(bool on)
{
    if (on)
    {
        SDL_ShowCursor(SDL_FALSE);
        if (s_canrelativemouse && userelativemouse)
        {
            if (SDL_SetRelativeMouseMode(SDL_TRUE) >= 0)
            {
                SDL_SetWindowGrab(screen, SDL_TRUE);
                s_relativemouse = true;
            }
            else
            {
                SDL_SetWindowGrab(screen, SDL_FALSE);
                s_canrelativemouse = false;
                s_relativemouse = false;
            }
        }
    }
    else
    {
        SDL_ShowCursor(SDL_TRUE);
        if (s_relativemouse)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(screen, SDL_FALSE);
            s_relativemouse = false;
        }
    }
    s_shouldgrab = false;
}

static void screenres(int w, int h)
{
    scr_w = clamp(w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(h, SCR_MINH, SCR_MAXH);
    if (screen)
    {
        scr_w = min(scr_w, desktopw);
        scr_h = min(scr_h, desktoph);
        if (SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)
        {
            gl_resize();
        }
        else
        {
            SDL_SetWindowSize(screen, scr_w, scr_h);
        }
    }
    else
    {
        initwarning("screen resolution");
    }
}

static void restoregamma()
{
    if (initing || s_curgamma == 100)
    {
        return;
    }
    setgamma(s_curgamma);
}

static void setupscreen()
{
    if (s_glcontext)
    {
        SDL_GL_DeleteContext(s_glcontext);
        s_glcontext = NULL;
    }
    if (screen)
    {
        SDL_DestroyWindow(screen);
        screen = NULL;
    }
    curvsync = -1;

    SDL_Rect desktop;
    if (SDL_GetDisplayBounds(0, &desktop) < 0)
    {
        fatal("failed querying desktop bounds: %s", SDL_GetError());
    }
    desktopw = desktop.w;
    desktoph = desktop.h;

    if (scr_h < 0)
    {
        scr_h = SCR_DEFAULTH;
    }
    if (scr_w < 0)
    {
        scr_w = (scr_h * desktopw) / desktoph;
    }
    scr_w = min(scr_w, desktopw);
    scr_h = min(scr_h, desktoph);

    const int winx = SDL_WINDOWPOS_UNDEFINED;
    const int winy = SDL_WINDOWPOS_UNDEFINED;
    int winw = scr_w;
    int winh = scr_h;
    int flags = SDL_WINDOW_RESIZABLE;
    if (fullscreen)
    {
        winw = desktopw;
        winh = desktoph;
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        s_initwindowpos = true;
    }

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    screen = SDL_CreateWindow(
        "Resseract",
        winx,
        winy,
        winw,
        winh,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | flags);
    if (!screen)
    {
        fatal("failed to create OpenGL window: %s", SDL_GetError());
    }

    SDL_SetWindowMinimumSize(screen, SCR_MINW, SCR_MINH);
    SDL_SetWindowMaximumSize(screen, SCR_MAXW, SCR_MAXH);

#ifdef __APPLE__
    static const int glversions[] = {32, 20};
#else
    static const int glversions[] = {40, 33, 32, 31, 30, 20};
#endif
    loopi(sizeof(glversions) / sizeof(glversions[0]))
    {
        glcompat = glversions[i] <= 30 ? 1 : 0;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glversions[i] / 10);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glversions[i] % 10);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, glversions[i] >= 32 ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
        s_glcontext = SDL_GL_CreateContext(screen);
        if (s_glcontext)
        {
            break;
        }
    }
    if (!s_glcontext)
    {
        fatal("failed to create OpenGL context: %s", SDL_GetError());
    }

    SDL_GetWindowSize(screen, &screenw, &screenh);
    renderw = min(scr_w, screenw);
    renderh = min(scr_h, screenh);
    hudw = screenw;
    hudh = screenh;
}

static void pushevent(const SDL_Event &e)
{
    s_events.add(e);
}

static void checkinput()
{
    SDL_Event event;
    // int lasttype = 0, lastbut = 0;
    bool mousemoved = false;
    while (s_events.length() || pollevent(event))
    {
        if (s_events.length())
        {
            event = s_events.remove(0);
        }

        switch (event.type)
        {
            case SDL_QUIT:
                quit();
                return;

            case SDL_TEXTINPUT:
                if (s_textinputmask && int(event.text.timestamp - s_textinputtime) >= textinputfilter)
                {
                    uchar buf[SDL_TEXTINPUTEVENT_TEXT_SIZE + 1];
                    size_t len =
                        decodeutf8(buf, sizeof(buf) - 1, (const uchar *)event.text.text, strlen(event.text.text));
                    if (len > 0)
                    {
                        buf[len] = '\0';
                        processtextinput((const char *)buf, len);
                    }
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (s_keyrepeatmask || !event.key.repeat)
                {
                    processkey(event.key.keysym.sym, event.key.state == SDL_PRESSED);
                }
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        quit();
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        s_shouldgrab = true;
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                        inputgrab(grabinput = true);
                        break;

                    case SDL_WINDOWEVENT_LEAVE:
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        inputgrab(grabinput = false);
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        minimized = true;
                        break;

                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        minimized = false;
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        break;

                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        SDL_GetWindowSize(screen, &screenw, &screenh);
                        if (!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
                        {
                            scr_w = clamp(screenw, SCR_MINW, SCR_MAXW);
                            scr_h = clamp(screenh, SCR_MINH, SCR_MAXH);
                        }
                        gl_resize();
                        break;
                }
                break;

            case SDL_MOUSEMOTION:
                if (grabinput)
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);
                    if (!UI::movecursor(dx, dy))
                    {
                        mousemove(dx, dy);
                    }
                    mousemoved = true;
                }
                else if (s_shouldgrab)
                    inputgrab(grabinput = true);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                // if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                        processkey(-1, event.button.state == SDL_PRESSED);
                        break;
                    case SDL_BUTTON_MIDDLE:
                        processkey(-2, event.button.state == SDL_PRESSED);
                        break;
                    case SDL_BUTTON_RIGHT:
                        processkey(-3, event.button.state == SDL_PRESSED);
                        break;
                    case SDL_BUTTON_X1:
                        processkey(-6, event.button.state == SDL_PRESSED);
                        break;
                    case SDL_BUTTON_X2:
                        processkey(-7, event.button.state == SDL_PRESSED);
                        break;
                }
                // lasttype = event.type;
                // lastbut = event.button.button;
                break;

            case SDL_MOUSEWHEEL:
                if (event.wheel.y > 0)
                {
                    processkey(-4, true);
                    processkey(-4, false);
                }
                else if (event.wheel.y < 0)
                {
                    processkey(-5, true);
                    processkey(-5, false);
                }
                break;
        }
    }
    if (mousemoved)
    {
        resetmousemotion();
    }
}

static void swapbuffers(bool overlay = true)
{
    recorder::capture(overlay);
    gle::disable();
    SDL_GL_SwapWindow(screen);
}

static void limitfps(int &millis, int curmillis)
{
    int limit = (mainmenu || minimized) && menufps ? (maxfps ? min(maxfps, menufps) : menufps) : maxfps;
    if (!limit)
    {
        return;
    }
    static int fpserror = 0;
    int delay = 1000 / limit - (millis - curmillis);
    if (delay < 0)
    {
        fpserror = 0;
    }
    else
    {
        fpserror += 1000 % limit;
        if (fpserror >= limit)
        {
            ++delay;
            fpserror -= limit;
        }
        if (delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

static void resetfpshistory()
{
    loopi(MAXFPSHISTORY) s_fpshistory[i] = 1;
    s_fpspos = 0;
}

static void updatefpshistory(int millis)
{
    s_fpshistory[s_fpspos++] = max(1, min(1000, millis));
    if (s_fpspos >= MAXFPSHISTORY)
    {
        s_fpspos = 0;
    }
}

static void getfps_(int *raw)
{
    if (*raw)
    {
        floatret(1000.0f / s_fpshistory[(s_fpspos + MAXFPSHISTORY - 1) % MAXFPSHISTORY]);
    }
    else
    {
        int fps, bestdiff, worstdiff;
        getfps(fps, bestdiff, worstdiff);
        intret(fps);
    }
}

static void showusage(const char *prgname)
{
    std::printf("Usage: %s [options] [game options]\n\n", prgname);
    std::printf("Options:\n");
    std::printf(" -?, --help   Show this help\n");
    std::printf(" -u<PATH>     Set the home directory\n");
    std::printf(" -g<FILE>     Set the log file\n");
    std::printf(" -k<PATH>     Add a package directory\n");
    std::printf(" -d[MODE]     Set dedicated mode (0, 1 or 2)\n");
    std::printf(" -w<WIDTH>    Set screen width\n");
    std::printf(" -h<HEIGHT>   Set screen height\n");
    std::printf(" -f<MODE>     Set fullscreen mode (0 or 1)\n");
    std::printf(" -l<FILE>     Load a specific map\n");
    std::printf(" -x<FILE>     Run a custom init script\n");
}

#if defined(_WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
static void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if (!ep)
    {
        fatal("unknown type");
    }
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    char out[512];
    formatstring(out,
                 "Resseract Win32 Exception: 0x%x [0x%x]\n\n",
                 er->ExceptionCode,
                 er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#ifdef _AMD64_
    STACKFRAME64 sf = {
        {context->Rip, 0, AddrModeFlat}, {}, {context->Rbp, 0, AddrModeFlat}, {context->Rsp, 0, AddrModeFlat}, 0};
    while (::StackWalk64(IMAGE_FILE_MACHINE_AMD64,
                         GetCurrentProcess(),
                         GetCurrentThread(),
                         &sf,
                         context,
                         NULL,
                         ::SymFunctionTableAccess,
                         ::SymGetModuleBase,
                         NULL))
    {
        union {
            IMAGEHLP_SYMBOL64 sym;
            char symext[sizeof(IMAGEHLP_SYMBOL64) + sizeof(string)];
        };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(line);
        DWORD64 symoff;
        DWORD lineoff;
        if (SymGetSymFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) &&
            SymGetLineFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#else
    STACKFRAME sf = {
        {context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    while (::StackWalk(IMAGE_FILE_MACHINE_I386,
                       GetCurrentProcess(),
                       GetCurrentThread(),
                       &sf,
                       context,
                       NULL,
                       ::SymFunctionTableAccess,
                       ::SymGetModuleBase,
                       NULL))
    {
        union {
            IMAGEHLP_SYMBOL sym;
            char symext[sizeof(IMAGEHLP_SYMBOL) + sizeof(string)];
        };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE line;
        line.SizeOfStruct = sizeof(line);
        DWORD symoff, lineoff;
        if (SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) &&
            SymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#endif
        {
            char *del = strrchr(line.FileName, '\\');
            concformatstring(out, "%s - %s [%d]\n", sym.Name, del ? del + 1 : line.FileName, line.LineNumber);
        }
    }
    fatal(out);
}
#endif

//-------------------------------------------------------------------------------------------------
// Public functions.
//-------------------------------------------------------------------------------------------------

void fatal(const char *s, ...)  // failure exit
{
    static int errors = 0;
    errors++;

    if (errors <= 2)  // print up to one extra recursive error
    {
        defvformatstring(msg, s, s);
        logoutf("%s", msg);

        if (errors <= 1)  // avoid recursion
        {
            if (SDL_WasInit(SDL_INIT_VIDEO))
            {
                SDL_ShowCursor(SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                if (screen)
                {
                    SDL_SetWindowGrab(screen, SDL_FALSE);
                }
                cleargamma();
#ifdef __APPLE__
                if (screen)
                {
                    SDL_SetWindowFullscreen(screen, 0);
                }
#endif
            }
            SDL_Quit();
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Resseract fatal error", msg, NULL);
        }
    }

    exit(EXIT_FAILURE);
}

bool initwarning(const char *desc, int level, int type)
{
    if (initing < level)
    {
        addchange(desc, type);
        return true;
    }
    return false;
}

void renderbackground(const char *caption, Texture *mapshot, const char *mapname, const char *mapinfo, bool force)
{
    if (!inbetweenframes && !force)
    {
        return;
    }

    if (menumute)
    {
        // Stop sounds while loading.
        stopsounds();
    }

    int w = hudw, h = hudh;
    if (forceaspect)
    {
        w = int(ceil(h * forceaspect));
    }
    getbackgroundres(w, h);
    gettextres(w, h);

    if (force)
    {
        renderbackgroundview(w, h, caption, mapshot, mapname, mapinfo);
        return;
    }

    loopi(3)
    {
        renderbackgroundview(w, h, caption, mapshot, mapname, mapinfo);
        swapbuffers(false);
    }

    setbackgroundinfo(caption, mapshot, mapname, mapinfo);
}

void renderprogress(float bar, const char *text, bool background)  // also used during loading
{
    if (!inbetweenframes || drawtex)
    {
        return;
    }

    clientkeepalive();  // make sure our connection doesn't time out while loading maps etc.

#ifdef __APPLE__
    interceptkey(SDLK_UNKNOWN);  // keep the event queue awake to avoid 'beachball' cursor
#endif

    int w = hudw, h = hudh;
    if (forceaspect)
    {
        w = int(ceil(h * forceaspect));
    }
    getbackgroundres(w, h);
    gettextres(w, h);

    bool forcebackground = progressbackground || (mesa_swap_bug && (curvsync || totalmillis == 1));
    if (background || forcebackground)
    {
        restorebackground(w, h, forcebackground);
    }

    renderprogressview(w, h, bar, text);
    swapbuffers(false);
}

void keyrepeat(bool on, int mask)
{
    if (on)
    {
        s_keyrepeatmask |= mask;
    }
    else
    {
        s_keyrepeatmask &= ~mask;
    }
}

void textinput(bool on, int mask)
{
    if (on)
    {
        if (!s_textinputmask)
        {
            SDL_StartTextInput();
            s_textinputtime = SDL_GetTicks();
        }
        s_textinputmask |= mask;
    }
    else
    {
        s_textinputmask &= ~mask;
        if (!s_textinputmask)
        {
            SDL_StopTextInput();
        }
    }
}

void resetgl()
{
    clearchanges(CHANGE_GFX | CHANGE_SHADERS);

    renderbackground("resetting OpenGL");

    recorder::cleanup();
    cleanupva();
    cleanupparticles();
    cleanupstains();
    cleanupsky();
    cleanupmodels();
    cleanupprefabs();
    cleanuptextures();
    cleanupblendmap();
    cleanuplights();
    cleanupshaders();
    cleanupgl();

    setupscreen();

    inputgrab(grabinput);

    gl_init();

    inbetweenframes = false;
    if (!reloadtexture(*notexture) ||
        !reloadtexture("<premul>media/interface/logo_512.png") ||
        !reloadtexture("<premul>media/interface/logo_1024.png") ||
        !reloadtexture("<premul>media/interface/logo_1500.png") ||
        !reloadtexture("media/interface/background.png") ||
        !reloadtexture("media/interface/shadow.png") ||
        !reloadtexture("media/interface/mapshot_frame.png") ||
        !reloadtexture("media/interface/loading_frame.png") ||
        !reloadtexture("media/interface/loading_bar.png"))
    {
        fatal("failed to reload core texture");
    }
    reloadfonts();
    inbetweenframes = true;
    renderbackground("initializing...");
    restoregamma();
    restorevsync();
    initgbuffer();
    reloadshaders();
    reloadtextures();
    allchanged(true);
}

bool interceptkey(int sym)
{
    static int lastintercept = SDLK_UNKNOWN;
    int len = lastintercept == sym ? s_events.length() : 0;
    SDL_Event event;
    while (pollevent(event))
    {
        switch (event.type)
        {
            case SDL_MOUSEMOTION:
                break;
            default:
                pushevent(event);
                break;
        }
    }
    lastintercept = sym;
    if (sym != SDLK_UNKNOWN)
    {
        for (int i = len; i < s_events.length(); i++)
        {
            if (s_events[i].type == SDL_KEYDOWN && s_events[i].key.keysym.sym == sym)
            {
                s_events.remove(i);
                return true;
            }
        }
    }
    return false;
}

void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = s_fpshistory[MAXFPSHISTORY - 1], best = total, worst = total;
    loopi(MAXFPSHISTORY - 1)
    {
        int millis = s_fpshistory[i];
        total += millis;
        if (millis < best)
        {
            best = millis;
        }
        if (millis > worst)
        {
            worst = millis;
        }
    }

    fps = (1000 * MAXFPSHISTORY) / total;
    bestdiff = 1000 / best - fps;
    worstdiff = fps - 1000 / worst;
}

int getclockmillis()
{
    int millis = static_cast<int>(SDL_GetTicks()) - s_clockrealbase;
    if (clockfix)
    {
        millis = static_cast<int>(static_cast<double>(millis) * (static_cast<double>(clockerror) / 1000000.0));
    }
    millis += s_clockvirtbase;
    return max(millis, totalmillis);
}

// On macOS SDL defines main as SDL_main.
#ifdef main
#undef main
#endif

int main(int argc, char **argv)
{
#if defined(_WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    __try
    {
#endif

    // Check if the user wants some help.
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-?") == 0) || (strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0) ||
            (strcmp(argv[i], "-help") == 0))
        {
            showusage(argv[0]);
            exit(0);
        }
    }

    setlogfile(NULL);

    int dedicated = 0;
    char *load = NULL, *initscript = NULL;

    initing = INIT_RESET;

    // Set home dir first.
    setdefaulthomedir();
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'u')
        {
            sethomedir(&argv[i][2]);
            break;
        }
    }
    logoutf("Using home directory: %s", homedir);

    // Set log after home dir, but before anything else.
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'g')
        {
            const char *file = argv[i][2] ? &argv[i][2] : "log.txt";
            setlogfile(file);
            logoutf("Setting log file: %s", file);
            break;
        }
    }

    // Run the init script.
    execfile("config/init.cfg", false);

    // Command line arguments override init.cfg.
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
                case 'u':
                    // Already handled.
                    break;
                case 'g':
                    // Already handled.
                    break;
                case 'k':
                {
                    const char *dir = addpackagedir(&argv[i][2]);
                    if (dir)
                    {
                        logoutf("Adding package directory: %s", dir);
                    }
                    break;
                }
                case 'd':
                    dedicated = atoi(&argv[i][2]);
                    if (dedicated <= 0)
                    {
                        dedicated = 2;
                    }
                    break;
                case 'w':
                    scr_w = clamp(atoi(&argv[i][2]), SCR_MINW, SCR_MAXW);
                    if (!findarg(argc, argv, "-h"))
                    {
                        scr_h = -1;
                    }
                    break;
                case 'h':
                    scr_h = clamp(atoi(&argv[i][2]), SCR_MINH, SCR_MAXH);
                    if (!findarg(argc, argv, "-w"))
                    {
                        scr_w = -1;
                    }
                    break;
                case 'f':
                    fullscreen = atoi(&argv[i][2]);
                    break;
                case 'l':
                {
                    char pkgdir[] = "media/";
                    load = strstr(path(&argv[i][2]), path(pkgdir));
                    if (load)
                    {
                        load += sizeof(pkgdir) - 1;
                    }
                    else
                    {
                        load = &argv[i][2];
                    }
                    break;
                }
                case 'x':
                    initscript = &argv[i][2];
                    break;
                default:
                    if (!serveroption(argv[i]))
                    {
                        gameargs.add(argv[i]);
                    }
                    break;
            }
        }
        else
        {
            gameargs.add(argv[i]);
        }
    }

    numcpus = clamp(SDL_GetCPUCount(), 1, 16);

    if (dedicated <= 1)
    {
        logoutf("init: sdl");

        if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
        {
            fatal("Unable to initialize SDL: %s", SDL_GetError());
        }
    }

    logoutf("init: net");
    if (enet_initialize() < 0)
    {
        fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);
    enet_time_set(0);

    logoutf("init: game");
    game::parseoptions(gameargs);
    initserver(dedicated > 0, dedicated > 1);  // never returns if dedicated
    ASSERT(dedicated <= 1);
    game::initclient();

    logoutf("init: video");
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
#if !defined(_WIN32) && !defined(__APPLE__)
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#endif
    setupscreen();
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput();  // workaround for spurious text-input events getting sent on first text input toggle?

    logoutf("init: gl");
    gl_checkextensions();
    gl_init();
    notexture = textureload("media/texture/game/notexture.png");
    if (!notexture)
    {
        fatal("could not find core textures");
    }

    logoutf("init: console");
    if (!execfile("config/stdlib.cfg", false))
    {
        // This is the first file we load.
        fatal("Cannot find data files (you are running from the wrong folder)");
    }
    if (!execfile("config/font.cfg", false))
    {
        fatal("Cannot find font definitions");
    }
    if (!setfont("default"))
    {
        fatal("No default font specified");
    }

    UI::setup();

    inbetweenframes = true;
    renderbackground("initializing...");

    logoutf("init: world");
    camera1 = player = game::iterdynents(0);
    emptymap(0, true, NULL, false);

    logoutf("init: sound");
    initsound();

    logoutf("init: cfg");
    initing = INIT_LOAD;
    execfile("config/keymap.cfg");
    execfile("config/stdedit.cfg");
    execfile(game::gameconfig());
    execfile("config/sound.cfg");
    execfile("config/ui.cfg");
    execfile("config/heightmap.cfg");
    execfile("config/blendbrush.cfg");
    if (game::savedservers())
    {
        execfile(game::savedservers(), false);
    }

    identflags |= IDF_PERSIST;

    if (!execfile(game::savedconfig(), false))
    {
        execfile(game::defaultconfig());
        writecfg(game::restoreconfig());
    }
    execfile(game::autoexec(), false);

    identflags &= ~IDF_PERSIST;

    initing = INIT_GAME;
    game::loadconfigs();

    initing = NOT_INITING;

    logoutf("init: render");
    restoregamma();
    restorevsync();
    initgbuffer();
    loadshaders();
    initparticles();
    initstains();

    identflags |= IDF_PERSIST;

    logoutf("init: mainloop");

    if (execfile("once.cfg", false))
    {
        remove(findfile("once.cfg", "rb"));
    }

    if (load)
    {
        logoutf("init: localconnect");
        // localconnect();
        game::changemap(load);
    }

    if (initscript)
    {
        execute(initscript);
    }

    initmumble();
    resetfpshistory();

    inputgrab(grabinput = true);
    ignoremousemotion();

    // Main game loop.
    int frames = 0;
    for (;;)
    {
        int millis = getclockmillis();
        limitfps(millis, totalmillis);
        elapsedtime = millis - totalmillis;
        static int timeerr = 0;
        int scaledtime = game::scaletime(elapsedtime) + timeerr;
        curtime = scaledtime / 100;
        timeerr = scaledtime % 100;
        if (!multiplayer(false) && curtime > 200)
        {
            curtime = 200;
        }
        if (game::ispaused())
        {
            curtime = 0;
        }
        lastmillis += curtime;
        totalmillis = millis;
        updatetime();

        checkinput();
        UI::update();
        menuprocess();
        tryedit();

        if (lastmillis)
            game::updateworld();

        checksleep(lastmillis);

        serverslice(false, 0);

        if (frames)
        {
            updatefpshistory(elapsedtime);
        }
        frames++;

        // miscellaneous general game effects
        recomputecamera();
        updateparticles();
        updatesounds();

        if (!minimized)
        {
            gl_setupframe(!mainmenu);

            inbetweenframes = false;
            gl_drawframe();
            swapbuffers();
            renderedframe = inbetweenframes = true;
        }
    }

    ASSERT(0);
    return EXIT_FAILURE;

#if defined(_WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    }
    __except (stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH)
    {
        return 0;
    }
#endif
}

// This is a quick workaround for making things run under macOS.
// TODO(m): Do something more fancy for parsing command line arguments etc.
#if defined(APPLE)
void _main()
{
    char *argv[2] = {"resseract", NULL};
    int argc = 1;
    main(argc, argv);
}
#endif

//-------------------------------------------------------------------------------------------------
// Exported commands.
//-------------------------------------------------------------------------------------------------

COMMAND(quit, "");
ICOMMAND(screenres, "ii", (int *w, int *h), screenres(*w, *h));
COMMAND(resetgl, "");
COMMANDN(getfps, getfps_, "i");
