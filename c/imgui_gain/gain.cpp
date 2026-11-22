/*
 * Gain: an Image Filter whose settings window is Dear ImGui in its own
 * Win32 + Direct3D 11 window (manual section 5, "Third-party UI toolkits").
 * Windows only. The pattern is the one proven in Layout 2025 by OFX Host:
 *
 *   - LWInterface.panel stays NULL, so Layout calls LWInterface.options()
 *     when the user opens the filter's settings; options() shows our window.
 *   - The window is owned by Layout's main window (Host Display Info) and
 *     drawn from a Win32 timer on the window. Layout's own message loop
 *     dispatches that timer and the mouse messages, so no thread is needed
 *     and every SDK call happens on Layout's main thread.
 *   - Layout's loop does not deliver key presses to a foreign window, so a
 *     WH_GETMESSAGE hook hands them over while our window is active.
 *   - The module's Shutdown removes the hook and the window before unload.
 */
#include <windows.h>
#include <d3d11.h>

#include <lwserver.h>
#include <lwmodule.h>
#include <lwhandler.h>
#include <lwfilter.h>
#include <lwdisplay.h>
#include <lwio.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include <new>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

// ---- the filter --------------------------------------------------------

struct Gain {
    float gain = 1.0f;   // read by process(); written by the UI on the main thread
};

GlobalFunc   *g_global;
LWInstUpdate *g_instUpdate;

LWInstance Gain_Create(void *, void *, LWError *err)
{
    Gain *g = new (std::nothrow) Gain;
    if (!g) *err = "Gain: out of memory";
    return g;
}

void ui_forget(Gain *g);
void Gain_Destroy(LWInstance in)
{
    ui_forget(static_cast<Gain *>(in));   // the window must never point at a freed instance
    delete static_cast<Gain *>(in);
}

LWError Gain_Copy(LWInstance to, LWInstance from)
{
    static_cast<Gain *>(to)->gain = static_cast<Gain *>(from)->gain;
    return nullptr;
}

LWError Gain_Save(LWInstance in, const LWSaveState *ss)
{
    int version = 1;
    LWSAVE_I4(ss, &version, 1);
    LWSAVE_FP(ss, &static_cast<Gain *>(in)->gain, 1);   // float: FP is right here
    return nullptr;
}

LWError Gain_Load(LWInstance in, const LWLoadState *ls)
{
    int version = 0;
    LWLOAD_I4(ls, &version, 1);
    if (version >= 1) LWLOAD_FP(ls, &static_cast<Gain *>(in)->gain, 1);
    return nullptr;
}

LWError Gain_Process(LWInstance in, const LWFilterAccess *fa)
{
    const float k = static_cast<Gain *>(in)->gain;
    for (int y = 0; y < fa->height; ++y) {
        const float *r = fa->getLine(LWBUFFER_BEAUTY, 0, y);
        const float *g = fa->getLine(LWBUFFER_BEAUTY, 1, y);
        const float *b = fa->getLine(LWBUFFER_BEAUTY, 2, y);
        if (!r || !g || !b) return "Gain: no beauty buffer";
        for (int x = 0; x < fa->width; ++x) {
            LWFVector out = { r[x] * k, g[x] * k, b[x] * k };
            fa->setRGB(x, y, out);
        }
    }
    return nullptr;
}

LWCStringASCII *Gain_Flags(LWInstance) { return nullptr; }

// ---- the ImGui window ----------------------------------------------------

const wchar_t *kClass   = L"LWEx_GainWindow";
const UINT_PTR kTimerId = 1;
const UINT     kTimerMs = 33;   // about 30 redraws a second

HWND                    g_wnd;
ImGuiContext           *g_imgui;
ID3D11Device           *g_dev;
ID3D11DeviceContext    *g_ctx;
IDXGISwapChain         *g_swap;
ID3D11RenderTargetView *g_rtv;
HHOOK                   g_keyHook;
Gain                   *g_shown;     // the instance the window edits, or null
bool                    g_inFrame;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void create_rtv()
{
    ID3D11Texture2D *back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) { g_dev->CreateRenderTargetView(back, nullptr, &g_rtv); back->Release(); }
}

void release_rtv() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

bool create_device(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount       = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = hwnd;
    sd.SampleDesc.Count  = 1;
    sd.Windowed          = TRUE;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (FAILED(hr))   // no usable GPU (remote desktop, VM): fall back to software
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (FAILED(hr)) return false;
    create_rtv();
    return true;
}

void release_device()
{
    release_rtv();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx)  { g_ctx->Release();  g_ctx  = nullptr; }
    if (g_dev)  { g_dev->Release();  g_dev  = nullptr; }
}

void draw_ui()
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##gain", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    if (!g_shown) {
        ImGui::TextDisabled("No Gain filter selected.");
    } else {
        // A Drag field, not a slider: with ConfigDragClickToInputText a single
        // click types a value, as LightWave's own fields do.
        if (ImGui::DragFloat("Gain", &g_shown->gain, 0.01f, 0.0f, 16.0f, "%.3f"))
            g_instUpdate(LWIMAGEFILTER_HCLASS, g_shown);   // tell Layout it changed
        if (ImGui::Button("Reset")) {
            g_shown->gain = 1.0f;
            g_instUpdate(LWIMAGEFILTER_HCLASS, g_shown);
        }
    }
    ImGui::End();
}

void render_frame()
{
    if (!g_wnd || !g_imgui || g_inFrame || IsIconic(g_wnd)) return;
    g_inFrame = true;   // a modal dialog pumps messages: never re-enter a frame
    ImGui::SetCurrentContext(g_imgui);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_ui();
    ImGui::Render();
    const float clear[4] = { 62 / 255.f, 62 / 255.f, 64 / 255.f, 1 };   // Layout's panel grey
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(1, 0);
    g_inFrame = false;
}

bool is_key_msg(UINT m)
{
    return m == WM_KEYDOWN || m == WM_KEYUP || m == WM_SYSKEYDOWN || m == WM_SYSKEYUP ||
           m == WM_CHAR || m == WM_SYSCHAR || m == WM_DEADCHAR || m == WM_UNICHAR;
}

// Layout's message loop does not deliver key messages to a foreign window.
// While our window is active, take them before Layout sees them, turn key
// presses into WM_CHAR with TranslateMessage, and call our window procedure.
// Function keys are left to Layout so F9 and F10 still render.
LRESULT CALLBACK KeyHook(int code, WPARAM wp, LPARAM lp)
{
    MSG *m = reinterpret_cast<MSG *>(lp);
    if (code == HC_ACTION && wp == PM_REMOVE && m && g_wnd && is_key_msg(m->message) &&
        (m->hwnd == g_wnd || GetActiveWindow() == g_wnd)) {
        const bool fkey = m->message != WM_CHAR && m->message != WM_SYSCHAR &&
                          m->wParam >= VK_F1 && m->wParam <= VK_F24;
        if (!fkey) {
            MSG copy = *m;
            copy.hwnd = g_wnd;
            m->message = WM_NULL;   // Layout sees nothing
            if (copy.message == WM_KEYDOWN || copy.message == WM_SYSKEYDOWN) TranslateMessage(&copy);
            WndProc(g_wnd, copy.message, copy.wParam, copy.lParam);
        }
    }
    return CallNextHookEx(g_keyHook, code, wp, lp);
}

HINSTANCE this_module()
{
    HMODULE h = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&WndProc), &h);
    return h;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_imgui) {
        ImGui::SetCurrentContext(g_imgui);
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    }
    switch (msg) {
    case WM_SIZE:
        if (g_swap && wp != SIZE_MINIMIZED) {
            release_rtv();
            g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            create_rtv();
            render_frame();
        }
        return 0;
    case WM_TIMER:
        if (wp == kTimerId && IsWindowVisible(hwnd)) render_frame();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        render_frame();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);   // keep it; options() shows it again
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;   // a lone Alt would enter the menu loop
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool create_window(HWND owner)
{
    WNDCLASSEXW wc = { sizeof wc };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = this_module();
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    // Owned by Layout's window: stays above it and minimises with it.
    g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClass, L"Gain", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 320, 140, owner, nullptr, wc.hInstance, nullptr);
    if (!g_wnd) return false;
    if (!create_device(g_wnd)) { DestroyWindow(g_wnd); g_wnd = nullptr; return false; }

    IMGUI_CHECKVERSION();
    ImGuiContext *prev = ImGui::GetCurrentContext();   // another plug-in may use ImGui too
    g_imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_imgui);
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;                 // write nothing into Layout's folder
    io.ConfigDragClickToInputText = true;     // click a number to type it
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_wnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    ImGui::SetCurrentContext(prev);

    SetTimer(g_wnd, kTimerId, kTimerMs, nullptr);
    if (!g_keyHook) g_keyHook = SetWindowsHookExW(WH_GETMESSAGE, KeyHook, nullptr, GetCurrentThreadId());
    return true;
}

void ui_open(Gain *g)
{
    HostDisplayInfo *hdi = static_cast<HostDisplayInfo *>(g_global(LWHOSTDISPLAYINFO_GLOBAL, GFUSE_TRANSIENT));
    g_shown = g;
    if (!g_wnd && !create_window(hdi ? hdi->window : nullptr)) return;
    ShowWindow(g_wnd, SW_SHOWNORMAL);
    SetForegroundWindow(g_wnd);
    render_frame();
}

void ui_forget(Gain *g) { if (g_shown == g) g_shown = nullptr; }

void ui_shutdown()
{
    if (g_keyHook) { UnhookWindowsHookEx(g_keyHook); g_keyHook = nullptr; }   // never leave Layout
    if (!g_wnd) return;                                                       // calling into an
    KillTimer(g_wnd, kTimerId);                                               // unloaded module
    if (g_imgui) {
        ImGui::SetCurrentContext(g_imgui);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(g_imgui);
        g_imgui = nullptr;
    }
    release_device();
    DestroyWindow(g_wnd);
    g_wnd = nullptr;
    UnregisterClassW(kClass, this_module());
    g_shown = nullptr;
}

// ---- activation ----------------------------------------------------------

LWError Gain_Options(LWInstance in)
{
    ui_open(static_cast<Gain *>(in));
    return nullptr;
}

int Gain_Activate(int version, GlobalFunc *global, void *local, void *)
{
    auto *h = static_cast<LWImageFilterHandler *>(local);
    if (version != LWIMAGEFILTER_VERSION) return AFUNC_BADVERSION;
    g_global = global;
    h->inst->create  = Gain_Create;
    h->inst->destroy = Gain_Destroy;
    h->inst->copy    = Gain_Copy;
    h->inst->load    = Gain_Load;
    h->inst->save    = Gain_Save;
    h->process       = Gain_Process;
    h->flags         = Gain_Flags;
    return AFUNC_OK;
}

int Gain_Interface(int version, GlobalFunc *global, void *local, void *)
{
    auto *ui = static_cast<LWInterface *>(local);
    if (version != LWINTERFACE_VERSION) return AFUNC_BADVERSION;
    g_global     = global;
    g_instUpdate = reinterpret_cast<LWInstUpdate *>(global(LWINSTUPDATE_GLOBAL, GFUSE_TRANSIENT));
    if (!g_instUpdate) return AFUNC_BADGLOBAL;
    ui->panel   = nullptr;        // no XPanel: Layout calls options() instead
    ui->options = Gain_Options;
    ui->command = nullptr;
    return AFUNC_OK;
}

ServerTagInfo Gain_Tags[] = {
    { "Gain (ImGui example)", SRVTAG_USERNAME | LANGID_USENGLISH },
    { nullptr, 0 }
};

void *Startup()        { return reinterpret_cast<void *>(1); }
void  Shutdown(void *) { ui_shutdown(); }

}  // namespace

extern "C" {
ServerRecord ServerDesc[] = {
    { LWIMAGEFILTER_HCLASS, "LWEx_GainImGui", Gain_Activate,  Gain_Tags },
    { LWIMAGEFILTER_ICLASS, "LWEx_GainImGui", Gain_Interface, Gain_Tags },
    { nullptr }
};

__declspec(dllexport) ModuleDescriptor _mod_descrip = {
    MOD_SYSSYNC, MOD_SYSVER, MOD_MACHINE, Startup, Shutdown, ServerDesc
};
}
