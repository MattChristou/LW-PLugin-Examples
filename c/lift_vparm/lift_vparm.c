/*
 * Lift with an animatable distance: the VParm version (manual section 3,
 * "Animatable parameters"). Click E beside the field to keyframe it.
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwmotion.h>
#include <lwrender.h>
#include <lwvparm.h>
#include <lwxpanel.h>
#include <lwio.h>
#include <stdlib.h>

static LWVParmFuncs *g_vparm;
static LWItemInfo   *g_iteminfo;

typedef struct { LWItemID item; LWVParmID vpLift; } Lift;

static LWInstance Lift_Create(void *priv, void *context, LWError *err)
{
    Lift  *inst = calloc(1, sizeof *inst);
    double init[3] = { 1.0, 1.0, 1.0 };
    (void)priv;
    if (!inst) { *err = "Lift: out of memory"; return NULL; }
    inst->item   = (LWItemID)context;
    inst->vpLift = g_vparm->create(LWVP_DIST, LWVPDT_NOTXTR);
    if (!inst->vpLift) { free(inst); *err = "Lift: no VParm"; return NULL; }
    g_vparm->setup(inst->vpLift, "Lift", g_iteminfo->chanGroup(inst->item),
                   NULL, NULL, "Lift", inst);
    g_vparm->setVal(inst->vpLift, init);
    return inst;
}

static void Lift_Destroy(LWInstance in)
{
    Lift *inst = in;
    if (inst->vpLift) g_vparm->destroy(inst->vpLift);
    free(inst);
}

/* Copy the VParm's contents, never the handle: each instance owns its own. */
static LWError Lift_Copy(LWInstance to, LWInstance from)
{
    return g_vparm->copy(((Lift *)to)->vpLift, ((Lift *)from)->vpLift);
}

static LWError Lift_Save(LWInstance in, const LWSaveState *ss)
{
    int version = 2;                      /* version 1 stored a plain double */
    LWSAVE_I4(ss, &version, 1);
    return g_vparm->save(((Lift *)in)->vpLift, ss);
}

static LWError Lift_Load(LWInstance in, const LWLoadState *ls)
{
    Lift *inst = in;
    int   version = 0;
    LWLOAD_I4(ls, &version, 1);
    if (version >= 2) return g_vparm->load(inst->vpLift, ls);
    if (version == 1) {                   /* upgrade an old scene */
        double v[3];
        LWLOAD_DP(ls, &v[0], 1);
        v[1] = v[2] = v[0];
        g_vparm->setVal(inst->vpLift, v);
    }
    return NULL;
}

static LWCStringUTF8 Lift_Describe(LWInstance in) { (void)in; return "Lift (animatable)"; }

static void Lift_Evaluate(LWInstance in, const LWItemMotionAccess *ma)
{
    LWDVector pos;
    double    lift[3];
    g_vparm->getVal(((Lift *)in)->vpLift, ma->time, NULL, lift);
    ma->getParam(LWIP_POSITION, ma->time, pos);
    pos[1] += lift[0];
    ma->setParam(LWIP_POSITION, pos);
}

static unsigned int Lift_Flags(LWInstance in) { (void)in; return 0; }

XCALL_(static int) Lift_Activate(int version, GlobalFunc *global,
                                 void *local, void *serverData)
{
    LWItemMotionHandler *h = (LWItemMotionHandler *)local;
    (void)serverData;
    if (version != LWITEMMOTION_VERSION) return AFUNC_BADVERSION;
    g_vparm    = global(LWVPARMFUNCS_GLOBAL, GFUSE_TRANSIENT);
    g_iteminfo = global(LWITEMINFO_GLOBAL,   GFUSE_TRANSIENT);
    if (!g_vparm || !g_iteminfo) return AFUNC_BADGLOBAL;

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

/* ---- interface: an envelope control bound to the VParm ------------------ */

#define ID_LIFT 0x4C460001u

static LWXPanelControl  Lift_Ctrl[] = { { ID_LIFT, "Lift", "distance-env" }, { 0 } };
static LWXPanelDataDesc Lift_Data[] = { { ID_LIFT, "Lift", "distance-env" }, { 0 } };

static void *Lift_Get(void *in, unsigned int vid)
{
    if (vid == ID_LIFT) return ((Lift *)in)->vpLift;    /* the handle itself */
    return NULL;
}

static LWXPRefreshCode Lift_Set(void *in, unsigned int vid, void *value)
{
    (void)in; (void)value;
    return vid == ID_LIFT ? LWXPRC_DFLT : LWXPRC_NONE;  /* never setVal here */
}

XCALL_(static int) Lift_Interface(int version, GlobalFunc *global,
                                  void *local, void *serverData)
{
    LWInterface   *ui = (LWInterface *)local;
    LWXPanelFuncs *xpf;
    (void)serverData;
    if (version != LWINTERFACE_VERSION) return AFUNC_BADVERSION;
    xpf = global(LWXPANELFUNCS_GLOBAL, GFUSE_TRANSIENT);
    if (!xpf) return AFUNC_BADGLOBAL;

    ui->panel = xpf->create(LWXP_VIEW, Lift_Ctrl);
    xpf->describe(ui->panel, Lift_Data, Lift_Get, Lift_Set);
    xpf->viewInst(ui->panel, ui->inst);
    xpf->setData(ui->panel, 0, ui->inst);
    ui->options = NULL;
    ui->command = NULL;
    return AFUNC_OK;
}

static ServerTagInfo Lift_Tags[] = {
    { "Lift (animatable)", SRVTAG_USERNAME | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWITEMMOTION_HCLASS, "LWEx_LiftVParm", Lift_Activate,  Lift_Tags },
    { LWITEMMOTION_ICLASS, "LWEx_LiftVParm", Lift_Interface, Lift_Tags },
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
