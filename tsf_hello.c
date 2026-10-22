/*
 * Hello-world TSF text service in plain C.
 * Press F9 in any text field -> inserts "Hello world!".
 *
 * Build (64-bit tcc):
 *   x86_64-win32-tcc -shared hello_tsf.c hello_tsf.def -o hello_tsf.dll ^
 *        -lole32 -luser32 -ladvapi32
 * Register (admin):  regsvr32 hello_tsf.dll
 * Then add "Hello C TIP" under Settings > Time & Language > Language > Keyboards,
 * select it, and press F9.
 *
 * GUIDs / vtable slot order written from memory of msctf.h - verify against the SDK.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <wchar.h>

#define COMAPI HRESULT __stdcall
typedef DWORD TfClientId, TfEditCookie;

#define DEF_GUID(n,l,w1,w2,a,b,c,d,e,f,g,h) \
    static const GUID n = {l,w1,w2,{a,b,c,d,e,f,g,h}}

/* TCC has no uuid library, so define the standard IIDs ourselves.
   Not static: the headers declare these as extern. */
const GUID IID_IUnknown      = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IClassFactory = {0x00000001,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

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
DEF_GUID(CLSID_HelloTip,   0x5B8E2A10,0x91C4,0x4E7D,0xA3,0x6F,0x0D,0x42,0x7B,0x19,0xC8,0x55);
DEF_GUID(GUID_HelloProfile,0x9E4D7C21,0x3B5A,0x4F80,0x82,0x1C,0x6A,0xD0,0x35,0xE7,0x4B,0x12);

#define IFACE(n) typedef struct n n; typedef struct n##Vtbl n##Vtbl; \
                 struct n { const n##Vtbl *lpVtbl; }
IFACE(ITfThreadMgr); IFACE(ITfKeystrokeMgr); IFACE(ITfKeyEventSink);
IFACE(ITfTextInputProcessor); IFACE(ITfEditSession); IFACE(ITfContext);
IFACE(ITfRange); IFACE(ITfInsertAtSelection);
IFACE(ITfInputProcessorProfiles); IFACE(ITfCategoryMgr);

#define IUNK(T) HRESULT (__stdcall *QueryInterface)(T*, REFIID, void**); \
                ULONG (__stdcall *AddRef)(T*); ULONG (__stdcall *Release)(T*)

struct ITfThreadMgrVtbl { IUNK(ITfThreadMgr); };
struct ITfKeystrokeMgrVtbl {
    IUNK(ITfKeystrokeMgr);
    HRESULT (__stdcall *AdviseKeyEventSink)(ITfKeystrokeMgr*, TfClientId, ITfKeyEventSink*, BOOL);
    HRESULT (__stdcall *UnadviseKeyEventSink)(ITfKeystrokeMgr*, TfClientId);
};
struct ITfKeyEventSinkVtbl {
    IUNK(ITfKeyEventSink);
    HRESULT (__stdcall *OnSetFocus)(ITfKeyEventSink*, BOOL);
    HRESULT (__stdcall *OnTestKeyDown)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    HRESULT (__stdcall *OnTestKeyUp)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    HRESULT (__stdcall *OnKeyDown)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    HRESULT (__stdcall *OnKeyUp)(ITfKeyEventSink*, ITfContext*, WPARAM, LPARAM, BOOL*);
    HRESULT (__stdcall *OnPreservedKey)(ITfKeyEventSink*, ITfContext*, REFGUID, BOOL*);
};
struct ITfTextInputProcessorVtbl {
    IUNK(ITfTextInputProcessor);
    HRESULT (__stdcall *Activate)(ITfTextInputProcessor*, ITfThreadMgr*, TfClientId);
    HRESULT (__stdcall *Deactivate)(ITfTextInputProcessor*);
};
struct ITfEditSessionVtbl {
    IUNK(ITfEditSession);
    HRESULT (__stdcall *DoEditSession)(ITfEditSession*, TfEditCookie);
};
typedef struct { ITfRange *range; int ase; BOOL fInterimChar; } TF_SELECTION;
struct ITfContextVtbl {
    IUNK(ITfContext);
    HRESULT (__stdcall *RequestEditSession)(ITfContext*, TfClientId, ITfEditSession*, DWORD, HRESULT*); /* 3 */
    void *_p1[2];                                                                             /* 4,5 */
    HRESULT (__stdcall *SetSelection)(ITfContext*, TfEditCookie, ULONG, const TF_SELECTION*);            /* 6 */
};
struct ITfRangeVtbl {
    IUNK(ITfRange);
    void *_p1[12];                                                                            /* 3..14 */
    HRESULT (__stdcall *Collapse)(ITfRange*, TfEditCookie, int);                                         /* 15 */
};
struct ITfInsertAtSelectionVtbl {
    IUNK(ITfInsertAtSelection);
    HRESULT (__stdcall *InsertTextAtSelection)(ITfInsertAtSelection*, TfEditCookie, DWORD,
                                    const WCHAR*, LONG, ITfRange**);
};
struct ITfInputProcessorProfilesVtbl {
    IUNK(ITfInputProcessorProfiles);
    HRESULT (__stdcall *Register)(ITfInputProcessorProfiles*, REFCLSID);
    HRESULT (__stdcall *Unregister)(ITfInputProcessorProfiles*, REFCLSID);
    HRESULT (__stdcall *AddLanguageProfile)(ITfInputProcessorProfiles*, REFCLSID, LANGID, REFGUID,
                                 const WCHAR*, ULONG, const WCHAR*, ULONG, ULONG);
};
struct ITfCategoryMgrVtbl {
    IUNK(ITfCategoryMgr);
    HRESULT (__stdcall *RegisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);
    HRESULT (__stdcall *UnregisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);
};

#define TF_ES_SYNC       0x1
#define TF_ES_READWRITE  0x6
#define TF_ANCHOR_END    1

/* ---------------------------------------------------------- globals */
static HINSTANCE     g_hinst;
static ITfThreadMgr *g_tmgr;
static TfClientId    g_client;
static ITfContext   *g_ctx;                 /* context for the pending edit */
static const WCHAR   g_text[] = L"Hello world!";

/* ------------------------------------------------ edit session (write) */
static COMAPI Es_QI(ITfEditSession *s, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfEditSession)) { *pp = s; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Es_AddRef(ITfEditSession *s)  { return 1; }
static ULONG __stdcall Es_Release(ITfEditSession *s) { return 1; }

static COMAPI Es_Do(ITfEditSession *s, TfEditCookie ec)
{
    ITfInsertAtSelection *ins = NULL;
    ITfRange *range = NULL;
    TF_SELECTION sel;

    if (FAILED(g_ctx->lpVtbl->QueryInterface(g_ctx, &IID_ITfInsertAtSelection, (void**)&ins)) || !ins)
        return S_OK;

    if (SUCCEEDED(ins->lpVtbl->InsertTextAtSelection(ins, ec, 0, g_text,
                                                     (LONG)wcslen(g_text), &range)) && range) {
        /* put the caret just after the inserted text (same as the SDK sample) */
        range->lpVtbl->Collapse(range, ec, TF_ANCHOR_END);
        sel.range = range; sel.ase = 0 /* TF_AE_NONE */; sel.fInterimChar = FALSE;
        g_ctx->lpVtbl->SetSelection(g_ctx, ec, 1, &sel);
        range->lpVtbl->Release(range);
    }
    ins->lpVtbl->Release(ins);
    return S_OK;
}
static const ITfEditSessionVtbl g_esVtbl = { Es_QI, Es_AddRef, Es_Release, Es_Do };
static ITfEditSession g_session = { &g_esVtbl };

/* ------------------------------------------------------ key event sink */
static COMAPI Ks_QI(ITfKeyEventSink *s, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfKeyEventSink)) { *pp = s; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Ks_AddRef(ITfKeyEventSink *s)  { return 1; }
static ULONG __stdcall Ks_Release(ITfKeyEventSink *s) { return 1; }
static COMAPI Ks_OnSetFocus(ITfKeyEventSink *s, BOOL f) { return S_OK; }

static COMAPI Ks_OnTestKeyDown(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *eaten)
{
    *eaten = (w == VK_F9);          /* tell TSF we want F9 */
    return S_OK;
}

static COMAPI Ks_OnKeyDown(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *eaten)
{
    HRESULT hrSession = E_FAIL;
    *eaten = FALSE;
    if (w != VK_F9) return S_OK;

    *eaten = TRUE;
    g_ctx = c;
    /* all document access must go through an edit session */
    c->lpVtbl->RequestEditSession(c, g_client, &g_session,
                                  TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    return S_OK;
}

static COMAPI Ks_OnTestKeyUp(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{ *e = (w == VK_F9); return S_OK; }   /* swallow matching key-up too */
static COMAPI Ks_OnKeyUp(ITfKeyEventSink *s, ITfContext *c, WPARAM w, LPARAM l, BOOL *e)
{ *e = (w == VK_F9); return S_OK; }
static COMAPI Ks_OnPreservedKey(ITfKeyEventSink *s, ITfContext *c, REFGUID g, BOOL *e)
{ *e = FALSE; return S_OK; }

static const ITfKeyEventSinkVtbl g_ksVtbl = {
    Ks_QI, Ks_AddRef, Ks_Release, Ks_OnSetFocus,
    Ks_OnTestKeyDown, Ks_OnTestKeyUp, Ks_OnKeyDown, Ks_OnKeyUp, Ks_OnPreservedKey
};
static ITfKeyEventSink g_keySink = { &g_ksVtbl };

/* ------------------------------------------------ text input processor */
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
    if (SUCCEEDED(tm->lpVtbl->QueryInterface(tm, &IID_ITfKeystrokeMgr, (void**)&ksm)) && ksm) {
        ksm->lpVtbl->AdviseKeyEventSink(ksm, id, &g_keySink, TRUE);
        ksm->lpVtbl->Release(ksm);
    }
    return S_OK;
}
static COMAPI Tip_Deactivate(ITfTextInputProcessor *t)
{
    if (g_tmgr) {
        ITfKeystrokeMgr *ksm = NULL;
        if (SUCCEEDED(g_tmgr->lpVtbl->QueryInterface(g_tmgr, &IID_ITfKeystrokeMgr, (void**)&ksm)) && ksm) {
            ksm->lpVtbl->UnadviseKeyEventSink(ksm, g_client);
            ksm->lpVtbl->Release(ksm);
        }
        g_tmgr->lpVtbl->Release(g_tmgr);
        g_tmgr = NULL;
    }
    return S_OK;
}
static const ITfTextInputProcessorVtbl g_tipVtbl = { Tip_QI, Tip_AddRef, Tip_Release, Tip_Activate, Tip_Deactivate };
static ITfTextInputProcessor g_tip = { &g_tipVtbl };

/* ---------------------------------------------------- class factory */
typedef struct CF CF;
struct CFVtbl {
    HRESULT (__stdcall *QueryInterface)(CF*, REFIID, void**);
    ULONG (__stdcall *AddRef)(CF*);
    ULONG (__stdcall *Release)(CF*);
    HRESULT (__stdcall *CreateInstance)(CF*, IUnknown*, REFIID, void**);
    HRESULT (__stdcall *LockServer)(CF*, BOOL);
};
struct CF { const struct CFVtbl *lpVtbl; };

static COMAPI Cf_QI(CF *f, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory)) { *pp = f; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Cf_AddRef(CF *f)  { return 1; }
static ULONG __stdcall Cf_Release(CF *f) { return 1; }
static COMAPI Cf_Create(CF *f, IUnknown *outer, REFIID iid, void **pp)
{ if (outer) return CLASS_E_NOAGGREGATION; return Tip_QI(&g_tip, iid, pp); }
static COMAPI Cf_Lock(CF *f, BOOL l) { return S_OK; }
static const struct CFVtbl g_cfVtbl = { Cf_QI, Cf_AddRef, Cf_Release, Cf_Create, Cf_Lock };
static CF g_factory = { &g_cfVtbl };

/* --------------------------------------------------------- exports */
/* TCC would export these as _Name@N; asm labels force the plain names regsvr32 expects. */
__declspec(dllexport) HRESULT __stdcall DllGetClassObject(REFCLSID, REFIID, void**) __asm__("DllGetClassObject");
__declspec(dllexport) HRESULT __stdcall DllCanUnloadNow(void)       __asm__("DllCanUnloadNow");
__declspec(dllexport) HRESULT __stdcall DllRegisterServer(void)     __asm__("DllRegisterServer");
__declspec(dllexport) HRESULT __stdcall DllUnregisterServer(void)   __asm__("DllUnregisterServer");

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{ if (reason == DLL_PROCESS_ATTACH) { g_hinst = h; DisableThreadLibraryCalls(h); } return TRUE; }

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
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfInputProcessorProfiles, (void**)&prof))) {
        prof->lpVtbl->Register(prof, &CLSID_HelloTip);
        prof->lpVtbl->AddLanguageProfile(prof, &CLSID_HelloTip, 0x0409, &GUID_HelloProfile,
                                         desc, (ULONG)wcslen(desc), path, (ULONG)wcslen(path), 0);
        prof->lpVtbl->Release(prof);
    }
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void**)&cat))) {
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
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void**)&cat))) {
        cat->lpVtbl->UnregisterCategory(cat, &CLSID_HelloTip, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_HelloTip);
        cat->lpVtbl->Release(cat);
    }
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfInputProcessorProfiles, (void**)&prof))) {
        prof->lpVtbl->Unregister(prof, &CLSID_HelloTip);
        prof->lpVtbl->Release(prof);
    }
    CoUninitialize();

    StringFromGUID2(&CLSID_HelloTip, clsid, 64);
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid); RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    wsprintfW(key, L"CLSID\\%s", clsid);                 RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    return S_OK;
}