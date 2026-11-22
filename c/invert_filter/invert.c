/*
 * Invert: an Image Filter that inverts the finished frame's colours
 * (manual section 4).
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwfilter.h>
#include <stdlib.h>

static LWInstance Invert_Create(void *priv, void *context, LWError *err)
{
    (void)priv; (void)context; (void)err;
    return (LWInstance)calloc(1, 1);      /* no settings, but must be non-NULL */
}

static void    Invert_Destroy(LWInstance in) { free(in); }
static LWError Invert_Copy(LWInstance to, LWInstance from) { (void)to; (void)from; return NULL; }

static LWError Invert_Process(LWInstance in, const LWFilterAccess *fa)
{
    (void)in;
    for (int y = 0; y < fa->height; ++y) {
        const float *r = fa->getLine(LWBUFFER_BEAUTY, 0, y);
        const float *g = fa->getLine(LWBUFFER_BEAUTY, 1, y);
        const float *b = fa->getLine(LWBUFFER_BEAUTY, 2, y);
        if (!r || !g || !b) return "Invert: no beauty buffer";
        for (int x = 0; x < fa->width; ++x) {
            LWFVector out = { 1.0f - r[x], 1.0f - g[x], 1.0f - b[x] };
            fa->setRGB(x, y, out);
        }
    }
    return NULL;
}

static LWCStringASCII *Invert_Flags(LWInstance in) { (void)in; return NULL; }

XCALL_(static int) Invert_Activate(int version, GlobalFunc *global,
                                   void *local, void *serverData)
{
    LWImageFilterHandler *h = (LWImageFilterHandler *)local;
    (void)global; (void)serverData;
    if (version != LWIMAGEFILTER_VERSION) return AFUNC_BADVERSION;

    h->inst->create  = Invert_Create;
    h->inst->destroy = Invert_Destroy;
    h->inst->copy    = Invert_Copy;
    h->process       = Invert_Process;
    h->flags         = Invert_Flags;
    return AFUNC_OK;
}

static ServerTagInfo Invert_Tags[] = {
    { "Invert (example)", SRVTAG_USERNAME | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWIMAGEFILTER_HCLASS, "LWEx_Invert", Invert_Activate, Invert_Tags },
    { NULL }
};

static void *Startup(void)        { return (void *)1; }
static void  Shutdown(void *data) { (void)data; }

#ifdef _WIN32
__declspec(dllexport)
#endif
ModuleDescriptor _mod_descrip = {
    MOD_SYSSYNC, MOD_SYSVER, MOD_MACHINE, Startup, Shutdown, ServerDesc
};
