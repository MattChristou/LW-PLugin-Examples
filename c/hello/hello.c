#include <lwserver.h>
#include <lwmodule.h>
#include <lwgeneric.h>
#include <lwhost.h>

static void *Startup(void)        { return (void *)1; }
static void  Shutdown(void *data) { (void)data; }

XCALL_(static int) Hello_Activate(int version, GlobalFunc *global,
                                  void *local, void *serverData)
{
    LWMessageFuncs *msg;
    (void)local; (void)serverData;
    if (version != LWLAYOUTGENERIC_VERSION) return AFUNC_BADVERSION;
    msg = global(LWMESSAGEFUNCS_GLOBAL, GFUSE_TRANSIENT);
    if (!msg) return AFUNC_BADGLOBAL;
    msg->info("Hello from C.", NULL);
    return AFUNC_OK;
}

static ServerTagInfo Hello_Tags[] = {
    { "Hello C",               SRVTAG_USERNAME   | LANGID_USENGLISH },
    { "Hello C",               SRVTAG_BUTTONNAME | LANGID_USENGLISH },
    { "Shows a greeting",      SRVTAG_DESCRIPTION | LANGID_USENGLISH },
    { NULL }
};

ServerRecord ServerDesc[] = {
    { LWLAYOUTGENERIC_CLASS, "LWEx_HelloC", Hello_Activate, Hello_Tags },
    { NULL }
};

#ifdef _WIN32
__declspec(dllexport)
#endif
ModuleDescriptor _mod_descrip = {
    MOD_SYSSYNC, MOD_SYSVER, MOD_MACHINE, Startup, Shutdown, ServerDesc
};
