/*
 * Lift: an Item Motion handler that raises an item by a fixed distance.
 * Manual sections 3 (anatomy) and 5 (XPanel interface).
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwmotion.h>
#include <lwio.h>
#include <lwxpanel.h>
#include <stdlib.h>
#include <stdio.h>

/* Callbacks are static in the plug-in and visible to the unit test. */
#ifdef LIFT_TESTING
#define PLUGIN_API
#else
#define PLUGIN_API static
#endif

typedef struct { LWItemID item; double lift; char desc[64]; } Lift;

PLUGIN_API LWInstance Lift_Create(void *priv, void *context, LWError *err)
{
    Lift *inst = calloc(1, sizeof *inst);
    if (!inst) { *err = "Lift: out of memory"; return NULL; }
    inst->item = (LWItemID)context;
    inst->lift = 1.0;
    return inst;
}

PLUGIN_API void Lift_Destroy(LWInstance in) { free(in); }

static LWError Lift_Copy(LWInstance to, LWInstance from)
{
    ((Lift *)to)->lift = ((Lift *)from)->lift;
    return NULL;
}

static LWError Lift_Save(LWInstance in, const LWSaveState *ss)
{
    int version = 1;
    LWSAVE_I4(ss, &version, 1);
    LWSAVE_DP(ss, &((Lift *)in)->lift, 1);
    return NULL;
}

static LWError Lift_Load(LWInstance in, const LWLoadState *ls)
{
    int version = 0;
    LWLOAD_I4(ls, &version, 1);
    if (version >= 1) LWLOAD_DP(ls, &((Lift *)in)->lift, 1);
    return NULL;
}

static LWCStringUTF8 Lift_Describe(LWInstance in)
{
    Lift *inst = in;
    snprintf(inst->desc, sizeof inst->desc, "Lift %.2f m", inst->lift);
    return inst->desc;
}

PLUGIN_API void Lift_Evaluate(LWInstance in, const LWItemMotionAccess *ma)
{
    LWDVector pos;
    ma->getParam(LWIP_POSITION, ma->time, pos);
    pos[1] += ((Lift *)in)->lift;
    ma->setParam(LWIP_POSITION, pos);
}

static unsigned int Lift_Flags(LWInstance in) { (void)in; return 0; }

XCALL_(static int) Lift_Activate(int version, GlobalFunc *global,
                                 void *local, void *serverData)
{
    LWItemMotionHandler *h = (LWItemMotionHandler *)local;
    (void)global; (void)serverData;
    if (version != LWITEMMOTION_VERSION) return AFUNC_BADVERSION;

    h->inst->create  = Lift_Create;
    h->inst->destroy = Lift_Destroy;
    h->inst->copy    = Lift_Copy;
    h->inst->load    = Lift_Load;
    h->inst->save    = Lift_Save;
    h->inst->descln  = Lift_Describe;
    h->evaluate      = Lift_Evaluate;
    h->flags         = Lift_Flags;
    return AFUNC_OK;
}

/* ---- interface (section 5) ---------------------------------------------- */

#define ID_BASE 0x4C460000u              /* far from LightWave's own IDs */
enum { ID_LIFT = ID_BASE + 1 };

static LWXPanelControl  Lift_Ctrl[] = { { ID_LIFT, "Lift", "distance" }, { 0 } };
static LWXPanelDataDesc Lift_Data[] = { { ID_LIFT, "Lift", "distance" }, { 0 } };

static LWInstUpdate *g_instUpdate;

static void *Lift_Get(void *in, unsigned int vid)
{
    Lift *inst = in;
    if (vid == ID_LIFT) return &inst->lift;
    return NULL;
}

static LWXPRefreshCode Lift_Set(void *in, unsigned int vid, void *value)
{
    Lift *inst = in;
    if (vid != ID_LIFT) return LWXPRC_NONE;
    inst->lift = *(double *)value;
    g_instUpdate(LWITEMMOTION_HCLASS, inst);
    return LWXPRC_DFLT;
}

XCALL_(static int) Lift_Interface(int version, GlobalFunc *global,
                                  void *local, void *serverData)
{
    LWInterface   *ui = (LWInterface *)local;
    LWXPanelFuncs *xpf;
    (void)serverData;

    if (version != LWINTERFACE_VERSION) return AFUNC_BADVERSION;
    xpf          = global(LWXPANELFUNCS_GLOBAL, GFUSE_TRANSIENT);
    g_instUpdate = global(LWINSTUPDATE_GLOBAL,  GFUSE_TRANSIENT);
    if (!xpf || !g_instUpdate) return AFUNC_BADGLOBAL;

    ui->panel = xpf->create(LWXP_VIEW, Lift_Ctrl);
    xpf->describe(ui->panel, Lift_Data, Lift_Get, Lift_Set);
    xpf->viewInst(ui->panel, ui->inst);
    xpf->setData(ui->panel, 0, ui->inst);
    ui->options = NULL;
    ui->command = NULL;
    return AFUNC_OK;
}

/* ---- registration ------------------------------------------------------- */

static ServerTagInfo Lift_Tags[] = {
    { "Lift",                              SRVTAG_USERNAME    | LANGID_USENGLISH },
    { "Raises an item by a fixed amount",  SRVTAG_DESCRIPTION | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWITEMMOTION_HCLASS, "LWEx_Lift", Lift_Activate,  Lift_Tags },
    { LWITEMMOTION_ICLASS, "LWEx_Lift", Lift_Interface, Lift_Tags },
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
