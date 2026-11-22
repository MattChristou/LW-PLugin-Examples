/*
 * Mesh Centroid: a Layout Generic that reports the centre of the selected
 * object's deformed points, and its first point's UV in a map named "UVMap"
 * (manual section 4, "Reading meshes in Layout").
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwgeneric.h>
#include <lwhost.h>
#include <lwrender.h>
#include <lwmeshes.h>
#include <stdio.h>

typedef struct { LWMeshInfoID mi; double sum[3]; int n; LWPntID first; } Centroid;

static size_t AddPoint(void *data, LWPntID pnt)
{
    Centroid *c = data;
    LWFVector p;
    c->mi->pntOtherPos(c->mi, pnt, p);
    c->sum[0] += p[0];  c->sum[1] += p[1];  c->sum[2] += p[2];
    if (c->n == 0) c->first = pnt;
    c->n++;
    return 0;                                   /* 0 = keep scanning */
}

static int PointUV(LWMeshInfoID mi, LWPntID pnt, LWPolID pol, const char *mapName, float uv[2])
{
    void *vmap = mi->pntVIDLookup(mi, LWVMAP_TXUV, mapName);
    if (!vmap) return 0;
    if (pol && mi->pntVPIDGet(mi, pnt, pol, uv, vmap)) return 1;
    return mi->pntVIDGet(mi, pnt, uv, vmap);
}

XCALL_(static int) Centroid_Activate(int version, GlobalFunc *global,
                                     void *local, void *serverData)
{
    LWMessageFuncs  *msg;
    LWInterfaceInfo *ui;
    LWObjectInfo    *objinfo;
    LWItemInfo      *iteminfo;
    LWItemID         obj;
    LWMeshInfoID     mi;
    Centroid         c = { 0 };
    char             line[256], uvline[128] = "No UVMap on the first point";
    float            uv[2];
    (void)local; (void)serverData;

    if (version != LWLAYOUTGENERIC_VERSION) return AFUNC_BADVERSION;
    msg      = global(LWMESSAGEFUNCS_GLOBAL,  GFUSE_TRANSIENT);
    ui       = global(LWINTERFACEINFO_GLOBAL, GFUSE_TRANSIENT);
    objinfo  = global(LWOBJECTINFO_GLOBAL,    GFUSE_TRANSIENT);
    iteminfo = global(LWITEMINFO_GLOBAL,      GFUSE_TRANSIENT);
    if (!msg || !ui || !objinfo || !iteminfo) return AFUNC_BADGLOBAL;

    obj = ui->selItems ? ui->selItems[0] : LWITEM_NULL;
    if (!obj || iteminfo->type(obj) != LWI_OBJECT) {
        msg->error("Select an object first.", NULL);
        return AFUNC_OK;
    }

    mi = objinfo->meshInfo(obj, 1);             /* 1 = deformed (frozen) mesh */
    if (!mi) { msg->error("The object has no mesh.", NULL); return AFUNC_OK; }
    c.mi = mi;
    mi->scanPoints(mi, AddPoint, &c);
    if (c.n > 0 && PointUV(mi, c.first, NULL, "UVMap", uv))
        snprintf(uvline, sizeof uvline, "First point UV: %.4f, %.4f", uv[0], uv[1]);
    if (mi->destroy) mi->destroy(mi);           /* usually NULL: Layout owns it */

    if (c.n == 0) { msg->info("The object has no points.", NULL); return AFUNC_OK; }
    snprintf(line, sizeof line, "%d points, centre %.4f, %.4f, %.4f",
             c.n, c.sum[0] / c.n, c.sum[1] / c.n, c.sum[2] / c.n);
    msg->info(line, uvline);
    return AFUNC_OK;
}

static ServerTagInfo Centroid_Tags[] = {
    { "Mesh Centroid (example)", SRVTAG_USERNAME   | LANGID_USENGLISH },
    { "Mesh Centroid",           SRVTAG_BUTTONNAME | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWLAYOUTGENERIC_CLASS, "LWEx_MeshCentroid", Centroid_Activate, Centroid_Tags },
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
