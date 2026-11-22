/*
 * Lift Points: a Modeler MeshEditTool. Drag left and right to lift the
 * selected points, or all points when none are selected (manual section 4).
 */
#include <lwserver.h>
#include <lwmodtool.h>
#include <lwmeshedt.h>
#include <lwmodeler.h>
#include <stdlib.h>
#include <lwmodule.h>

typedef struct { double lift, lift0; int update; } LiftTool;
typedef struct { MeshEditOp *op; double dy; int selOnly; } LiftScan;

static EDError LiftPoint(void *data, const EDPointInfo *pi)
{
    LiftScan *s = data;
    double    pos[3];
    if (s->selOnly && !(pi->flags & EDDF_SELECT)) return EDERR_NONE;
    pos[0] = pi->position[0];
    pos[1] = pi->position[1] + s->dy;
    pos[2] = pi->position[2];
    return s->op->pntMove(s->op->state, pi->pnt, pos);
}
static LWError LiftTool_Build(LWInstance in, MeshEditOp *op)
{
    LiftTool *t = in;
    LiftScan  s = { op, t->lift, 0 };
    EDError   err;
    s.selOnly = op->pointCount(op->state, OPLYR_FG, EDCOUNT_SELECT) > 0;
    err = op->pointScan(op->state, LiftPoint, &s, OPLYR_FG);
    op->done(op->state, err, 0);
    t->update = LWT_TEST_NOTHING;
    return err ? "Lift failed" : NULL;
}
static int  LiftTool_Test(LWInstance in)           { return ((LiftTool *)in)->update; }
static void LiftTool_End(LWInstance in, int keep)  { ((LiftTool *)in)->update = LWT_TEST_NOTHING; }
static void LiftTool_Done(LWInstance in)           { free(in); }
static int LiftTool_Down(LWInstance in, LWToolEvent *e)
{
    LiftTool *t = in;
    t->lift0  = t->lift;
    t->update = LWT_TEST_UPDATE;
    return 1;
}
static void LiftTool_Move(LWInstance in, LWToolEvent *e)
{
    LiftTool *t = in;
    t->lift   = t->lift0 + 0.01 * e->dx;
    t->update = LWT_TEST_UPDATE;
}
static LWCStringUTF8 LiftTool_Help(LWInstance in, LWToolEvent *e)
{
    return "Drag left or right to lift the points";
}
XCALL_(static int) LiftTool_Activate(int version, GlobalFunc *global,
                                     void *local, void *serverData)
{
    LWMeshEditTool *m = (LWMeshEditTool *)local;
    LiftTool       *t;
    if (version != LWMESHEDITTOOL_VERSION) return AFUNC_BADVERSION;
    t = calloc(1, sizeof *t);
    if (!t) return AFUNC_BADLOCAL;
    m->instance    = t;
    m->tool->done  = LiftTool_Done;
    m->tool->help  = LiftTool_Help;
    m->tool->down  = LiftTool_Down;
    m->tool->move  = LiftTool_Move;
    m->build       = LiftTool_Build;
    m->test        = LiftTool_Test;
    m->end         = LiftTool_End;
    return AFUNC_OK;
}

static ServerTagInfo LiftTool_Tags[] = {
    { "Lift Points (example)", SRVTAG_USERNAME   | LANGID_USENGLISH },
    { "Lift Points",           SRVTAG_BUTTONNAME | LANGID_USENGLISH },
    { "pnt",                   SRVTAG_ENABLE },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWMESHEDITTOOL_CLASS, "LWEx_LiftTool", LiftTool_Activate, LiftTool_Tags },
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
