/*
 * Checker: a minimal texture node with one input and one output
 * (manual section 6). Appears in the Node Editor under LWEx/Textures.
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwnodes.h>
#include <lwrender.h>
#include <math.h>
#include <stdlib.h>

static LWNodeInputFuncs  *g_in;
static LWNodeOutputFuncs *g_out;

typedef struct {
    NodeID       node;
    NodeInputID  inScale;
    NodeOutputID outColor;
    double       scale;
} Checker;

static LWInstance Checker_Create(void *priv, void *context, LWError *err)
{
    Checker *c = calloc(1, sizeof *c);
    (void)priv;
    if (!c) { *err = "Checker: out of memory"; return NULL; }
    c->node     = (NodeID)context;
    c->scale    = 1.0;
    c->inScale  = g_in->create(c->node, NOT_SCALAR, "Scale", NULL);
    c->outColor = g_out->create(c->node, NOT_RGB, "Color");
    return c;
}

static void Checker_Destroy(LWInstance in) { free(in); }

/* Copy settings only: each node keeps its own pins. */
static LWError Checker_Copy(LWInstance to, LWInstance from)
{
    ((Checker *)to)->scale = ((Checker *)from)->scale;
    return NULL;
}

static void Checker_Evaluate(LWInstance in, LWShadingGeometry *sg,
                             NodeOutputID out, NodeValue value)
{
    Checker  *c = in;
    double    s = c->scale;
    LWDVector col;
    int       k;
    (void)out;

    if (g_in->check(c->inScale)) g_in->evaluate(c->inScale, sg, &s);
    k = ((int)floor(sg->oP[0] * s) + (int)floor(sg->oP[1] * s)
       + (int)floor(sg->oP[2] * s)) & 1;
    col[0] = col[1] = col[2] = k ? 1.0 : 0.0;
    g_out->setValue(value, col);
}

XCALL_(static int) Checker_Activate(int version, GlobalFunc *global,
                                    void *local, void *serverData)
{
    LWNodeHandler *h = (LWNodeHandler *)local;
    (void)serverData;
    if (version != LWNODECLASS_VERSION) return AFUNC_BADVERSION;
    g_in  = global(LWNODEINPUTFUNCS_GLOBAL,  GFUSE_TRANSIENT);
    g_out = global(LWNODEOUTPUTFUNCS_GLOBAL, GFUSE_TRANSIENT);
    if (!g_in || !g_out) return AFUNC_BADGLOBAL;

    h->inst->create  = Checker_Create;
    h->inst->destroy = Checker_Destroy;
    h->inst->copy    = Checker_Copy;
    h->evaluate      = Checker_Evaluate;
    return AFUNC_OK;
}

static ServerTagInfo Checker_Tags[] = {
    { "Checker (example)", SRVTAG_USERNAME  | LANGID_USENGLISH },
    { "LWEx/Textures",     SRVTAG_NODEGROUP | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWNODE_HCLASS, "LWEx_Checker", Checker_Activate, Checker_Tags },
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
