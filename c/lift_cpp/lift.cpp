/*
 * Lift in C++17: instance trampolines, typed globals and one parameter
 * table driving the XPanel, Get/Set and save/load (manual section 3).
 */
#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwmotion.h>
#include <lwio.h>
#include <lwxpanel.h>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

// ---- typed globals -------------------------------------------------------
template <class T>
T *lwGlobal(GlobalFunc *g, const char *name)
{
    return static_cast<T *>(g(name, GFUSE_TRANSIENT));
}

// ---- instance trampolines: C callbacks -> C++ object ---------------------
template <class T>
struct Instance {
    static LWInstance create(void *, void *ctx, LWError *err) noexcept
    {
        try { return new T(static_cast<LWItemID>(ctx)); }
        catch (...) { *err = "Could not create instance"; return nullptr; }
    }
    static void destroy(LWInstance in) noexcept { delete static_cast<T *>(in); }
    static LWError copy(LWInstance to, LWInstance from) noexcept
    {
        try { static_cast<T *>(to)->copyFrom(*static_cast<T *>(from)); return nullptr; }
        catch (...) { return "Copy failed"; }
    }
    static LWError load(LWInstance in, const LWLoadState *ls) noexcept
    {
        try { return static_cast<T *>(in)->load(ls); } catch (...) { return "Load failed"; }
    }
    static LWError save(LWInstance in, const LWSaveState *ss) noexcept
    {
        try { return static_cast<T *>(in)->save(ss); } catch (...) { return "Save failed"; }
    }
    static LWCStringUTF8 descln(LWInstance in) noexcept
    {
        return static_cast<T *>(in)->describe();
    }
    static void fill(LWInstanceFuncs *f)
    {
        f->create = create;  f->destroy = destroy;  f->copy = copy;
        f->load   = load;    f->save    = save;     f->descln = descln;
    }
};

// ---- the handler ---------------------------------------------------------
struct Lift {
    explicit Lift(LWItemID item) : item(item) {}
    void          copyFrom(const Lift &o) { lift = o.lift; }
    LWError       load(const LWLoadState *ls);
    LWError       save(const LWSaveState *ss);
    const char   *describe();
    void          evaluate(const LWItemMotionAccess *ma) const;

    LWItemID    item;
    double      lift = 1.0;
    std::string desc;
};

// ---- one parameter table drives panel, get/set and save/load -------------
struct Param { unsigned id; const char *label; const char *type; double Lift::*field; };

constexpr unsigned ID_BASE = 0x4C460000u;
static const Param kParams[] = {
    { ID_BASE + 1, "Lift", "distance", &Lift::lift },
};

LWError Lift::save(const LWSaveState *ss)
{
    int version = 1;
    LWSAVE_I4(ss, &version, 1);
    for (const Param &p : kParams) LWSAVE_DP(ss, &(this->*p.field), 1);
    return nullptr;
}

LWError Lift::load(const LWLoadState *ls)
{
    int version = 0;
    LWLOAD_I4(ls, &version, 1);
    if (version >= 1)
        for (const Param &p : kParams) LWLOAD_DP(ls, &(this->*p.field), 1);
    return nullptr;
}

const char *Lift::describe()
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "Lift %.2f m", lift);
    desc = buf;
    return desc.c_str();
}

void Lift::evaluate(const LWItemMotionAccess *ma) const
{
    LWDVector pos;
    ma->getParam(LWIP_POSITION, ma->time, pos);
    pos[1] += lift;
    ma->setParam(LWIP_POSITION, pos);
}

static LWInstUpdate *g_instUpdate;

static void *Lift_Get(void *in, unsigned int vid)
{
    for (const Param &p : kParams)
        if (p.id == vid) return &(static_cast<Lift *>(in)->*p.field);
    return nullptr;
}

static LWXPRefreshCode Lift_Set(void *in, unsigned int vid, void *value)
{
    for (const Param &p : kParams)
        if (p.id == vid) {
            static_cast<Lift *>(in)->*p.field = *static_cast<double *>(value);
            g_instUpdate(LWITEMMOTION_HCLASS, in);
            return LWXPRC_DFLT;
        }
    return LWXPRC_NONE;
}

// Built once from kParams, so the control and data tables can never disagree.
static std::vector<LWXPanelControl>  g_ctrl;
static std::vector<LWXPanelDataDesc> g_data;

static void buildTables()
{
    if (!g_ctrl.empty()) return;
    for (const Param &p : kParams) {
        g_ctrl.push_back({ p.id, p.label, p.type });
        g_data.push_back({ p.id, p.label, p.type });
    }
    g_ctrl.push_back({});
    g_data.push_back({});
}

static int Lift_Activate(int version, GlobalFunc *, void *local, void *)
{
    auto *h = static_cast<LWItemMotionHandler *>(local);
    if (version != LWITEMMOTION_VERSION) return AFUNC_BADVERSION;
    Instance<Lift>::fill(h->inst);
    h->evaluate = [](LWInstance in, const LWItemMotionAccess *ma) noexcept {
        static_cast<Lift *>(in)->evaluate(ma);
    };
    h->flags = [](LWInstance) noexcept -> unsigned int { return 0; };
    return AFUNC_OK;
}

static int Lift_Interface(int version, GlobalFunc *global, void *local, void *)
{
    auto *ui = static_cast<LWInterface *>(local);
    if (version != LWINTERFACE_VERSION) return AFUNC_BADVERSION;
    auto *xpf    = lwGlobal<LWXPanelFuncs>(global, LWXPANELFUNCS_GLOBAL);
    g_instUpdate = lwGlobal<LWInstUpdate>(global, LWINSTUPDATE_GLOBAL);
    if (!xpf || !g_instUpdate) return AFUNC_BADGLOBAL;

    buildTables();
    ui->panel = xpf->create(LWXP_VIEW, g_ctrl.data());
    xpf->describe(ui->panel, g_data.data(), Lift_Get, Lift_Set);
    xpf->viewInst(ui->panel, ui->inst);
    xpf->setData(ui->panel, 0, ui->inst);
    ui->options = nullptr;
    ui->command = nullptr;
    return AFUNC_OK;
}

static ServerTagInfo Lift_Tags[] = {
    { "Lift (C++)", SRVTAG_USERNAME | LANGID_USENGLISH },
    { nullptr, 0 }
};

static void *Startup()       { return reinterpret_cast<void *>(1); }
static void  Shutdown(void *) {}

extern "C" {
ServerRecord ServerDesc[] = {
    { LWITEMMOTION_HCLASS, "LWEx_LiftCpp", Lift_Activate,  Lift_Tags },
    { LWITEMMOTION_ICLASS, "LWEx_LiftCpp", Lift_Interface, Lift_Tags },
    { nullptr }
};
#ifdef _WIN32
__declspec(dllexport)
#endif
ModuleDescriptor _mod_descrip = {
    MOD_SYSSYNC, MOD_SYSVER, MOD_MACHINE, Startup, Shutdown, ServerDesc
};
}
