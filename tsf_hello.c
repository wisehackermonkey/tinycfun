/*
 * Minimal TSF text service in plain C: press F9 -> inserts "hello world".
 *
 * Build (64-bit tcc):
 *   x86_64-win32-tcc -shared tsf_hello.c tsf_hello.def -o tsf_hello.dll ^
 *        -lole32 -luser32 -ladvapi32
 * Register (admin):  regsvr32 tsf_hello.dll
 * Then add "Hello C TIP" under Settings > Time & Language > Language > Keyboards,
 * switch to it, click in Notepad, press F9.
 *
 * tsf_hello.def:  LIBRARY tsf_hello / EXPORTS DllGetClassObject DllCanUnloadNow
 *                 DllRegisterServer DllUnregisterServer
 *
 * Vtable slot order and GUIDs are from memory of msctf.h - verify against the SDK.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <wchar.h>

#define COMAPI HRESULT __stdcall
typedef DWORD TfClientId, TfEditCookie;

#define DEF_GUID(n,l,w1,w2,a,b,c,d,e,f,g,h) \
    static const GUID n = {l,w1,w2,{a,b,c,d,e,f,g,h}}
DEF_GUID(IID_ITfKeystrokeMgr,       0xAA80E7F0,0x2021,0x11D2,0x93,0xE0,0x00,0x60,0xB0,0x67,0xB8,0x6E);
DEF_GUID(IID_ITfKeyEventSink,       0xAA80E7F5,0x2021,0x11D2,0x93,0xE0,0x00,0x60,0xB0,0x67,0xB8,0x6E);
DEF_GUID(IID_ITfTextInputProcessor, 0xAA80E7F7,0x2021,0x11D2,0x93,0xE0,0x00,0x60,0xB0,0x67,0xB8,0x6E);
DEF_GUID(IID_ITfEditSession,        0xAA80E803,0x2021,0x11D2,0x93,0xE0,0x00,0x60,0xB0,0x67,0xB8,0x6E);
DEF_GUID(IID_ITfInsertAtSelection,  0x55CE16BA,0x3014,0x41C1,0x9C,0xEB,0xFA,0xDE,0x14,0x46,0xAC,0x6C);
DEF_GUID(CLSID_TF_InputProcessorProfiles, 0x33C53A50,0xF456,0x4884,0xB0,0x49,0x85,0xFD,0x64,0x3E,0xCF,0xED);
DEF_GUID(IID_ITfInputProcessorProfiles,   0x1F02B6C5,0x7842,0x4EE6,0x8A,0x0B,0x9A,0x24,0x18,0x3A,0x95,0xCA);
DEF_GUID(CLSID_TF_CategoryMgr,      0xA4B544A1,0x438D,0x4B41,0x93,0x25,0x86,0x95,0x23,0xE2,0xD6,0xC7);
DEF_GUID(IID_ITfCategoryMgr,        0xC3ACEFB5,0xF69D,0x4905,0x93,0x8F,0xFC,0xAD,0xCF,0x4B,0xE8,0x30);
DEF_GUID(GUID_TFCAT_TIP_KEYBOARD,   0x34745C63,0xB2F0,0x4784,0x8B,0x67,0x5E,0x12,0xC8,0x70,0x1A,0x31);
/* our identity */
DEF_GUID(CLSID_HelloTip,   0x5E1A9C20,0x7B3D,0x4A18,0x8F,0x2C,0x91,0xD4,0x0B,0x6E,0x3A,0x77);
DEF_GUID(GUID_HelloProfile,0x2B8F4D61,0x1C9E,0x4E35,0xA7,0x0D,0x58,0x3F,0xE2,0x94,0x1B,0xC6);

/* ---- minimal C interface declarations ---- */
#define IFACE(n) typedef struct n n; typedef struct n##Vtbl n##Vtbl; \
                 struct n { const n##Vtbl *lpVtbl; }
#define IUNK(T) \
    COMAPI (*QueryInterface)(T*, REFIID, void**); \
    ULONG __stdcall (*AddRef)(T*); \
    ULONG __stdcall (*Release)(T*)

IFACE(ITfThreadMgr); IFACE(ITfKeystrokeMgr); IFACE(ITfKeyEventSink);
IFACE(ITfTextInputProcessor); IFACE(ITfEditSession); IFACE(ITfContext);
IFACE(ITfRange); IFACE(ITfInsertAtSelection);
IFACE(ITfInputProcessorProfiles); IFACE(ITfCategoryMgr);

struct ITfThreadMgrVtbl { IUNK(ITfThreadMgr); };  /* only QI needed here */

struct ITfKeystrokeMgrVtbl {
    IUNK(ITfKeystrokeMgr);
    COMAPI (*AdviseKeyEventSink)(ITfKeystrokeMgr*, TfClientId, ITfKeyEventSink*, BOOL);
    COMAPI (*UnadviseKeyEventSink)(ITfKeystrokeMgr*, TfClientId);
};
struct ITfKeyEventSinkVtbl {
    IUNK(ITfKeyEventSink);
    COMAPI (*OnSetFocus)(ITfKeyEventSink*, BOOL);
    COMAPI (*OnTestKeyDown)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    COMAPI (*OnTestKeyUp)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    COMAPI (*OnKeyDown)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    COMAPI (*OnKeyUp)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    COMAPI (*OnPreservedKey)(ITfKeyEventSink*, ITfContext*, REFGUID, BOOL*);
};
struct ITfTextInputProcessorVtbl {
    IUNK(ITfTextInputProcessor);
    COMAPI (*Activate)(ITfTextInputProcessor*, ITfThreadMgr*, TfClientId);
    COMAPI (*Deactivate)(ITfTextInputProcessor*);
};
struct ITfEditSessionVtbl {
    IUNK(ITfEditSession);
    COMAPI (*DoEditSession)(ITfEditSession*, TfEditCookie);
};
typedef struct { ITfRange *range; int ase; BOOL fInterimChar; } TF_SELECTION;
struct ITfContextVtbl {
    IUNK(ITfContext);
    COMAPI (*RequestEditSession)(ITfContext*, TfClientId, ITfEditSession*, DWORD, HRESULT*); /* 3 */
    void *_pad[2];                                                                           /* 4,5 */
    COMAPI (*SetSelection)(ITfContext*, TfEditCookie, ULONG, const TF_SELECTION*);           /* 6 */
};
struct ITfRangeVtbl {
    IUNK(ITfRange);
    void *_pad[12];                                                                          /* 3..14 */
    COMAPI (*Collapse)(ITfRange*, TfEditCookie, int);                                        /* 15 */
};
struct ITfInsertAtSelectionVtbl {
    IUNK(ITfInsertAtSelection);
    COMAPI (*InsertTextAtSelection)(ITfInsertAtSelection*, TfEditCookie, DWORD,
                                    const WCHAR*, LONG, ITfRange**);
};
struct ITfInputProcessorProfilesVtbl {
    IUNK(ITfInputProcessorProfiles);
    COMAPI (*Register)(ITfInputProcessorProfiles*, REFCLSID);
    COMAPI (*Unregister)(ITfInputProcessorProfiles*, REFCLSID);
    COMAPI (*AddLanguageProfile)(ITfInputProcessorProfiles*, REFCLSID, LANGID, REFGUID,
                                 const WCHAR*, ULONG, const WCHAR*, ULONG, ULONG);
};
struct ITfCategoryMgrVtbl {
    IUNK(ITfCategoryMgr);
    COMAPI (*RegisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);
    COMAPI (*UnregisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);
};

#define TF_ES_SYNC       0x1
#define TF_ES_READWRITE  0x6
#define TF_ANCHOR_END    1
#define TF_AE_NONE       0

/* ---- globals ---- */
static HINSTANCE     g_hinst;
static ITfThreadMgr *g_tmgr;
static TfClientId    g_client;
static ITfContext   *g_ctx;       /* context for the pending edit session */

/* ================= THE HELLO WORLD PART ================= */

/* Runs with a document write lock. All text access must happen here. */
static COMAPI Es_Do(ITfEditSession *s, TfEditCookie ec)
{
    static const WCHAR text[] = L"hello world";
    ITfInsertAtSelection *ins = NULL;
    ITfRange *range = NULL;
    TF_SELECTION sel;

    if (g_ctx->lpVtbl->QueryInterface(g_ctx, &IID_ITfInsertAtSelection, (void**)&ins) != S_OK)
        return S_OK;

    if (ins->lpVtbl->InsertTextAtSelection(ins, ec, 0, text, (LONG)wcslen(text), &range) == S_OK && range) {
        range->lpVtbl->Collapse(range, ec, TF_ANCHOR_END);   /* caret after inserted text */
        sel.range = range; sel.ase = TF_AE_NONE; sel.fInterimChar = FALSE;
        g_ctx->lpVtbl->SetSelection(g_ctx, ec, 1, &sel);
        range->lpVtbl->Release(range);
    }
    ins->lpVtbl->Release(ins);
    return S_OK;
}

static COMAPI Es_QI(ITfEditSession *s, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfEditSession)) { *pp = s; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Es_AddRef(ITfEditSession *s)  { return 1; }
static ULONG __stdcall Es_Release(ITfEditSession *s) { return 1; }
static const ITfEditSessionVtbl g_esVtbl = { Es_QI, Es_AddRef, Es_Release, Es_Do };
static ITfEditSession g_session = { &g_esVtbl };

static void InsertHello(ITfContext *ctx)
{
    HRESULT hr;
    g_ctx = ctx;
    ctx->lpVtbl->RequestEditSession(ctx, g_client, &g_session, TF_ES_SYNC | TF_ES_READWRITE, &hr);
}

/* ================= the "command": F9 ================= */

static COMAPI Ks_QI(ITfKeyEventSink *s, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfKeyEventSink)) { *pp = s; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Ks_AddRef(ITfKeyEventSink *s)  { return 1; }
static ULONG __stdcall Ks_Release(ITfKeyEventSink *s) { return 1; }
static COMAPI Ks_OnSetFocus(ITfKeyEventSink *s, BOOL f) { return S_OK; }
static COMAPI Ks_OnTestKeyDown(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{ *e = (w == VK_F9); return S_OK; }
static COMAPI Ks_OnKeyDown(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{
    *e = (w == VK_F9);
    if (*e) InsertHello(c);
    return S_OK;
}
static COMAPI Ks_OnTestKeyUp(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{ *e = (w == VK_F9); return S_OK; }   /* eat the matching key-up too */
static COMAPI Ks_OnKeyUp(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{ *e = (w == VK_F9); return S_OK; }
static COMAPI Ks_OnPreservedKey(ITfKeyEventSink *s, ITfContext *c, REFGUID g, BOOL *e)
{ *e = FALSE; return S_OK; }
static const ITfKeyEventSinkVtbl g_ksVtbl = {
    Ks_QI, Ks_AddRef, Ks_Release, Ks_OnSetFocus,
    Ks_OnTestKeyDown, Ks_OnTestKeyUp, Ks_OnKeyDown, Ks_OnKeyUp, Ks_OnPreservedKey
};
static ITfKeyEventSink g_keySink = { &g_ksVtbl };

/* ================= text input processor ================= */

static COMAPI Tip_QI(ITfTextInputProcessor *t, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfTextInputProcessor)) { *pp = t; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Tip_AddRef(ITfTextInputProcessor *t)  { return 1; }
static ULONG __stdcall Tip_Release(ITfTextInputProcessor *t) { return 1; }

static COMAPI Tip_Activate(ITfTextInputProcessor *t, ITfThreadMgr *tm, TfClientId id)
{
    ITfKeystrokeMgr *ksm = NULL;
    g_tmgr = tm; tm->lpVtbl->AddRef(tm);
    g_client = id;
    if (tm->lpVtbl->QueryInterface(tm, &IID_ITfKeystrokeMgr, (void**)&ksm) == S_OK && ksm) {
        ksm->lpVtbl->AdviseKeyEventSink(ksm, id, &g_keySink, TRUE);
        ksm->lpVtbl->Release(ksm);
    }
    return S_OK;
}
static COMAPI Tip_Deactivate(ITfTextInputProcessor *t)
{
    if (g_tmgr) {
        ITfKeystrokeMgr *ksm = NULL;
        if (g_tmgr->lpVtbl->QueryInterface(g_tmgr, &IID_ITfKeystrokeMgr, (void**)&ksm) == S_OK && ksm) {
            ksm->lpVtbl->UnadviseKeyEventSink(ksm, g_client);
            ksm->lpVtbl->Release(ksm);
        }
        g_tmgr->lpVtbl->Release(g_tmgr);
        g_tmgr = NULL;
    }
    return S_OK;
}
static const ITfTextInputProcessorVtbl g_tipVtbl = {
    Tip_QI, Tip_AddRef, Tip_Release, Tip_Activate, Tip_Deactivate
};
static ITfTextInputProcessor g_tip = { &g_tipVtbl };

/* ================= class factory + DLL exports ================= */

typedef struct IFactoryC { const struct IFactoryCVtbl *lpVtbl; } IFactoryC;
struct IFactoryCVtbl {
    COMAPI (*QueryInterface)(IFactoryC*, REFIID, void**);
    ULONG __stdcall (*AddRef)(IFactoryC*);
    ULONG __stdcall (*Release)(IFactoryC*);
    COMAPI (*CreateInstance)(IFactoryC*, IUnknown*, REFIID, void**);
    COMAPI (*LockServer)(IFactoryC*, BOOL);
};
static COMAPI Cf_QI(IFactoryC *f, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory)) { *pp = f; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Cf_AddRef(IFactoryC *f)  { return 1; }
static ULONG __stdcall Cf_Release(IFactoryC *f) { return 1; }
static COMAPI Cf_Create(IFactoryC *f, IUnknown *o, REFIID iid, void **pp)
{ if (o) return CLASS_E_NOAGGREGATION; return Tip_QI(&g_tip, iid, pp); }
static COMAPI Cf_Lock(IFactoryC *f, BOOL l) { return S_OK; }
static const struct IFactoryCVtbl g_cfVtbl = { Cf_QI, Cf_AddRef, Cf_Release, Cf_Create, Cf_Lock };
static IFactoryC g_factory = { &g_cfVtbl };

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p)
{ if (r == DLL_PROCESS_ATTACH) { g_hinst = h; DisableThreadLibraryCalls(h); } return TRUE; }

__declspec(dllexport) HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void **pp)
{
    if (!IsEqualCLSID(clsid, &CLSID_HelloTip)) { *pp = NULL; return CLASS_E_CLASSNOTAVAILABLE; }
    return Cf_QI(&g_factory, iid, pp);
}
__declspec(dllexport) HRESULT __stdcall DllCanUnloadNow(void) { return S_FALSE; }

__declspec(dllexport) HRESULT __stdcall DllRegisterServer(void)
{
    WCHAR clsid[64], key[256], path[MAX_PATH];
    const WCHAR *desc = L"Hello C TIP";
    HKEY hk;
    ITfInputProcessorProfiles *prof = NULL;
    ITfCategoryMgr *cat = NULL;

    StringFromGUID2(&CLSID_HelloTip, clsid, 64);
    GetModuleFileNameW(g_hinst, path, MAX_PATH);

    wsprintfW(key, L"CLSID\\%s", clsid);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, key, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == 0) {
        RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)desc, (DWORD)((wcslen(desc) + 1) * 2));
        RegCloseKey(hk);
    }
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, key, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == 0) {
        const WCHAR *tm = L"Apartment";
        RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)path, (DWORD)((wcslen(path) + 1) * 2));
        RegSetValueExW(hk, L"ThreadingModel", 0, REG_SZ, (BYTE*)tm, (DWORD)((wcslen(tm) + 1) * 2));
        RegCloseKey(hk);
    }

    CoInitialize(NULL);
    if (CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                         &IID_ITfInputProcessorProfiles, (void**)&prof) == S_OK) {
        prof->lpVtbl->Register(prof, &CLSID_HelloTip);
        prof->lpVtbl->AddLanguageProfile(prof, &CLSID_HelloTip, 0x0409, &GUID_HelloProfile,
                                         desc, (ULONG)wcslen(desc), path, (ULONG)wcslen(path), 0);
        prof->lpVtbl->Release(prof);
    }
    if (CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                         &IID_ITfCategoryMgr, (void**)&cat) == S_OK) {
        cat->lpVtbl->RegisterCategory(cat, &CLSID_HelloTip, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_HelloTip);
        cat->lpVtbl->Release(cat);
    }
    CoUninitialize();
    return S_OK;
}

__declspec(dllexport) HRESULT __stdcall DllUnregisterServer(void)
{
    WCHAR clsid[64], key[256];
    ITfInputProcessorProfiles *prof = NULL;
    ITfCategoryMgr *cat = NULL;

    CoInitialize(NULL);
    if (CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                         &IID_ITfCategoryMgr, (void**)&cat) == S_OK) {
        cat->lpVtbl->UnregisterCategory(cat, &CLSID_HelloTip, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_HelloTip);
        cat->lpVtbl->Release(cat);
    }
    if (CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                         &IID_ITfInputProcessorProfiles, (void**)&prof) == S_OK) {
        prof->lpVtbl->Unregister(prof, &CLSID_HelloTip);
        prof->lpVtbl->Release(prof);
    }
    CoUninitialize();

    StringFromGUID2(&CLSID_HelloTip, clsid, 64);
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid); RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    wsprintfW(key, L"CLSID\\%s", clsid);                 RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    return S_OK;
}