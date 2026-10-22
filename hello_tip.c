/*
 * Hello World TSF text service in plain C.
 * Only registers itself so it appears under Language > Keyboards.
 * Selecting it loads the DLL and calls Activate/Deactivate, which do nothing.
 *
 * Build (64-bit tcc):
 *   x86_64-win32-tcc -shared hello_tip.c hello_tip.def -o hello_tip.dll -lole32 -ladvapi32
 * Register (elevated prompt):  regsvr32 hello_tip.dll
 * Remove:                      regsvr32 /u hello_tip.dll
 *
 * Then: Settings > Time & Language > Language & region > English (US)
 *       > Language options > Add a keyboard > "Hello World TIP"
 * (If it doesn't show up immediately, sign out and back in.)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <wchar.h>

#define COMAPI HRESULT __stdcall

#define DEF_GUID(n,l,w1,w2,a,b,c,d,e,f,g,h) \
    static const GUID n = {l,w1,w2,{a,b,c,d,e,f,g,h}}

/* tcc has no uuid.lib: define the standard COM IIDs ourselves (non-static,
   because objbase.h declares them extern) */
const GUID IID_IUnknown      = {0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IClassFactory = {0x00000001,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

/* From msctf.h */
DEF_GUID(IID_ITfTextInputProcessor,       0xAA80E7F7,0x2021,0x11D2,0x93,0xE0,0x00,0x60,0xB0,0x67,0xB8,0x6E);
DEF_GUID(CLSID_TF_InputProcessorProfiles, 0x33C53A50,0xF456,0x4884,0xB0,0x49,0x85,0xFD,0x64,0x3E,0xCF,0xED);
DEF_GUID(IID_ITfInputProcessorProfiles,   0x1F02B6C5,0x7842,0x4EE6,0x8A,0x0B,0x9A,0x24,0x18,0x3A,0x95,0xCA);
DEF_GUID(IID_ITfInputProcessorProfileMgr, 0x71C6E74C,0x0F28,0x11D8,0xA8,0x2A,0x00,0x06,0x5B,0x84,0x43,0x5C);
DEF_GUID(CLSID_TF_CategoryMgr,            0xA4B544A1,0x438D,0x4B41,0x93,0x25,0x86,0x95,0x23,0xE2,0xD6,0xC7);
DEF_GUID(IID_ITfCategoryMgr,              0xC3ACEFB5,0xF69D,0x4905,0x93,0x8F,0xFC,0xAD,0xCF,0x4B,0xE8,0x30);
DEF_GUID(GUID_TFCAT_TIP_KEYBOARD,         0x34745C63,0xB2F0,0x4784,0x8B,0x67,0x5E,0x12,0xC8,0x70,0x1A,0x31);

/* Ours - generate your own for a real project */
DEF_GUID(CLSID_HelloTip,    0x7A3C1B52,0x4D0E,0x4F6A,0x9B,0x1D,0x2E,0x5C,0x8F,0x0A,0x6D,0x34);
DEF_GUID(GUID_HelloProfile, 0xC1D2E3F4,0x0A1B,0x4C2D,0x8E,0x3F,0x4A,0x5B,0x6C,0x7D,0x8E,0x9F);

#define IFACE(n) typedef struct n n; typedef struct n##Vtbl n##Vtbl; \
                 struct n { const n##Vtbl *lpVtbl; }
#define IUNK(T) COMAPI (*QueryInterface)(T*, REFIID, void**); \
                ULONG __stdcall (*AddRef)(T*); ULONG __stdcall (*Release)(T*)

IFACE(ITfThreadMgr);
IFACE(ITfTextInputProcessor);
IFACE(ITfInputProcessorProfiles);
IFACE(ITfInputProcessorProfileMgr);
IFACE(ITfCategoryMgr);

struct ITfTextInputProcessorVtbl {
    IUNK(ITfTextInputProcessor);
    COMAPI (*Activate)(ITfTextInputProcessor*, ITfThreadMgr*, DWORD);
    COMAPI (*Deactivate)(ITfTextInputProcessor*);
};
struct ITfInputProcessorProfilesVtbl {
    IUNK(ITfInputProcessorProfiles);
    COMAPI (*Register)(ITfInputProcessorProfiles*, REFCLSID);                       /* 3 */
    COMAPI (*Unregister)(ITfInputProcessorProfiles*, REFCLSID);                     /* 4 */
    COMAPI (*AddLanguageProfile)(ITfInputProcessorProfiles*, REFCLSID, LANGID, REFGUID,
                                 const WCHAR*, ULONG, const WCHAR*, ULONG, ULONG);  /* 5 */
};
struct ITfInputProcessorProfileMgrVtbl {
    IUNK(ITfInputProcessorProfileMgr);
    void *_pad[5];                                                                  /* 3..7 */
    COMAPI (*RegisterProfile)(ITfInputProcessorProfileMgr*, REFCLSID, LANGID, REFGUID,
                              const WCHAR*, ULONG, const WCHAR*, ULONG, ULONG,
                              HKL, DWORD, BOOL, DWORD);                             /* 8 */
    COMAPI (*UnregisterProfile)(ITfInputProcessorProfileMgr*, REFCLSID, LANGID,
                                REFGUID, DWORD);                                    /* 9 */
};
struct ITfCategoryMgrVtbl {
    IUNK(ITfCategoryMgr);
    COMAPI (*RegisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);        /* 3 */
    COMAPI (*UnregisterCategory)(ITfCategoryMgr*, REFCLSID, REFGUID, REFGUID);      /* 4 */
};

#define LANG_ID 0x0409   /* en-US; change to the language you want to appear under */
static const WCHAR g_desc[] = L"Hello World TIP";
static HINSTANCE g_hinst;

/* ---------------- the (empty) text service ---------------- */
static COMAPI Tip_QI(ITfTextInputProcessor *t, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ITfTextInputProcessor)) {
        *pp = t; return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Tip_AddRef(ITfTextInputProcessor *t)  { return 1; }
static ULONG __stdcall Tip_Release(ITfTextInputProcessor *t) { return 1; }
static COMAPI Tip_Activate(ITfTextInputProcessor *t, ITfThreadMgr *tm, DWORD id)
{
    OutputDebugStringW(L"HelloTip: Activate\n");   /* visible in DebugView */
    return S_OK;
}
static COMAPI Tip_Deactivate(ITfTextInputProcessor *t)
{
    OutputDebugStringW(L"HelloTip: Deactivate\n");
    return S_OK;
}
static const ITfTextInputProcessorVtbl g_tipVtbl =
    { Tip_QI, Tip_AddRef, Tip_Release, Tip_Activate, Tip_Deactivate };
static ITfTextInputProcessor g_tip = { &g_tipVtbl };

/* ---------------- class factory ---------------- */
typedef struct CF CF;
struct CFVtbl {
    COMAPI (*QueryInterface)(CF*, REFIID, void**);
    ULONG __stdcall (*AddRef)(CF*);
    ULONG __stdcall (*Release)(CF*);
    COMAPI (*CreateInstance)(CF*, IUnknown*, REFIID, void**);
    COMAPI (*LockServer)(CF*, BOOL);
};
struct CF { const struct CFVtbl *lpVtbl; };

static COMAPI Cf_QI(CF *f, REFIID iid, void **pp)
{
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory)) { *pp = f; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG __stdcall Cf_AddRef(CF *f)  { return 1; }
static ULONG __stdcall Cf_Release(CF *f) { return 1; }
static COMAPI Cf_Create(CF *f, IUnknown *o, REFIID iid, void **pp)
{
    if (o) return CLASS_E_NOAGGREGATION;
    return Tip_QI(&g_tip, iid, pp);
}
static COMAPI Cf_Lock(CF *f, BOOL l) { return S_OK; }
static const struct CFVtbl g_cfVtbl = { Cf_QI, Cf_AddRef, Cf_Release, Cf_Create, Cf_Lock };
static CF g_cf = { &g_cfVtbl };

/* ---------------- DLL exports ---------------- */
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    if (reason == DLL_PROCESS_ATTACH) g_hinst = h;
    return TRUE;
}

/* Takes 3 args so it must stay __stdcall; the asm label forces the plain export name */
__declspec(dllexport) HRESULT __stdcall DllGetClassObject(REFCLSID, REFIID, void**) __asm__("DllGetClassObject");
__declspec(dllexport) HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void **pp)
{
    if (!IsEqualCLSID(clsid, &CLSID_HelloTip)) { *pp = NULL; return CLASS_E_CLASSNOTAVAILABLE; }
    return Cf_QI(&g_cf, iid, pp);
}

__declspec(dllexport) HRESULT DllCanUnloadNow(void) { return S_FALSE; }

/* No arguments, so default (cdecl) == stdcall on x86, and tcc exports the plain name */
__declspec(dllexport) HRESULT DllRegisterServer(void)
{
    WCHAR clsid[64], key[256], path[MAX_PATH];
    HKEY hk;
    HRESULT hr;
    ITfInputProcessorProfileMgr *mgr = NULL;
    ITfInputProcessorProfiles *prof = NULL;
    ITfCategoryMgr *cat = NULL;

    StringFromGUID2(&CLSID_HelloTip, clsid, 64);
    GetModuleFileNameW(g_hinst, path, MAX_PATH);

    /* 1. COM registration: HKCR\CLSID\{...}\InprocServer32 */
    wsprintfW(key, L"CLSID\\%s", clsid);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, key, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == 0) {
        RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)g_desc, sizeof(g_desc));
        RegCloseKey(hk);
    }
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, key, 0, NULL, 0, KEY_WRITE, NULL, &hk, NULL) == 0) {
        static const WCHAR apt[] = L"Apartment";
        RegSetValueExW(hk, NULL, 0, REG_SZ, (BYTE*)path, (DWORD)((wcslen(path) + 1) * 2));
        RegSetValueExW(hk, L"ThreadingModel", 0, REG_SZ, (BYTE*)apt, sizeof(apt));
        RegCloseKey(hk);
    }

    /* 2. TSF profile registration (this is what makes it appear in the keyboard list) */
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                          &IID_ITfInputProcessorProfileMgr, (void**)&mgr);
    if (SUCCEEDED(hr)) {
        hr = mgr->lpVtbl->RegisterProfile(mgr, &CLSID_HelloTip, LANG_ID, &GUID_HelloProfile,
                                          g_desc, (ULONG)wcslen(g_desc),
                                          NULL, 0, 0,        /* no icon */
                                          NULL, 0, TRUE, 0); /* enabled by default */
        mgr->lpVtbl->Release(mgr);
    } else {
        /* Pre-Windows 8 fallback */
        hr = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                              &IID_ITfInputProcessorProfiles, (void**)&prof);
        if (SUCCEEDED(hr)) {
            prof->lpVtbl->Register(prof, &CLSID_HelloTip);
            hr = prof->lpVtbl->AddLanguageProfile(prof, &CLSID_HelloTip, LANG_ID, &GUID_HelloProfile,
                                                  g_desc, (ULONG)wcslen(g_desc), NULL, 0, 0);
            prof->lpVtbl->Release(prof);
        }
    }

    /* 3. Mark it as a keyboard-category TIP */
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void**)&cat))) {
        cat->lpVtbl->RegisterCategory(cat, &CLSID_HelloTip, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_HelloTip);
        cat->lpVtbl->Release(cat);
    }
    CoUninitialize();
    return hr;
}

__declspec(dllexport) HRESULT DllUnregisterServer(void)
{
    WCHAR clsid[64], key[256];
    ITfInputProcessorProfileMgr *mgr = NULL;
    ITfCategoryMgr *cat = NULL;

    CoInitialize(NULL);
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void**)&cat))) {
        cat->lpVtbl->UnregisterCategory(cat, &CLSID_HelloTip, &GUID_TFCAT_TIP_KEYBOARD, &CLSID_HelloTip);
        cat->lpVtbl->Release(cat);
    }
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfInputProcessorProfileMgr, (void**)&mgr))) {
        mgr->lpVtbl->UnregisterProfile(mgr, &CLSID_HelloTip, LANG_ID, &GUID_HelloProfile, 0);
        mgr->lpVtbl->Release(mgr);
    }
    CoUninitialize();

    StringFromGUID2(&CLSID_HelloTip, clsid, 64);
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid); RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    wsprintfW(key, L"CLSID\\%s", clsid);                 RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    return S_OK;
}