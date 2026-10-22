/*
 * tipdemo.c - a small but complete Text Services Framework text service in C.
 *
 *   - Registers itself as a keyboard in Settings > Language
 *   - Captures a-z into a private buffer
 *   - Shows a suggestion popup next to the caret
 *   - READS the text before the caret (shown in the popup footer)
 *   - WRITES the chosen suggestion into the focused app
 *
 * Keys while typing:
 *   a-z        add to buffer           Backspace  delete last char
 *   Up/Down    move selection          1-9        pick that suggestion
 *   Space      commit selection + " "  Enter      commit selection
 *   Esc        cancel
 *
 * Uses the real MinGW-w64 <msctf.h> (C interface, via COBJMACROS).
 * Build:  see Makefile (w64devkit:  make)
 */

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <msctf.h>
#include <wchar.h>

/* ------------------------------------------------------------ identity */
/* Generate your own GUIDs for a real project (guidgen / uuidgen / PowerShell [guid]::NewGuid()) */
static const GUID CLSID_SampleTip =
    {0xB6A1F3C2,0x5D47,0x4E89,{0xA0,0xB1,0x7C,0x2D,0x9E,0x3F,0x4A,0x60}};
static const GUID GUID_SampleProfile =
    {0xE4F5A6B7,0x1C2D,0x4E3F,{0x8A,0x9B,0x0C,0x1D,0x2E,0x3F,0x4A,0x5B}};

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL ((TfClientId)0)
#endif

#define TIP_DESC     L"Sample C Suggestions"
#define TIP_LANGID   0x0409          /* en-US: shows up under English (United States) */

#define MAX_BUF      32
#define MAX_CAND     9
#define BEFORE_CHARS 12

static HINSTANCE g_hinst;
static LONG      g_dllRefs;          /* objects alive + server locks */

static void Log(const WCHAR *s) { OutputDebugStringW(s); }

/* ------------------------------------------------------ suggestion list */
static const WCHAR *const g_words[] = {
    L"hello", L"help", L"helmet", L"helicopter", L"world", L"word", L"work",
    L"works", L"workflow", L"window", L"windows", L"text", L"service",
    L"framework", L"input", L"method", L"editor", L"example", L"candidate",
    L"suggestion", L"keyboard", L"language", L"compile", L"compiler", NULL
};

/* ----------------------------------------------------------- main object */
typedef struct Tip {
    ITfTextInputProcessor tip;       /* interface 1 */
    ITfKeyEventSink       sink;      /* interface 2 */
    LONG                  ref;

    ITfThreadMgr         *tm;
    TfClientId            client;
    HWND                  popup;

    WCHAR                 buf[MAX_BUF];       /* what the user typed */
    int                   len;
    const WCHAR          *cand[MAX_CAND];     /* current suggestions */
    int                   ncand, sel;
    WCHAR                 before[BEFORE_CHARS + 1]; /* text read before caret */
    POINT                 caret;              /* screen position for popup */
} Tip;

#define TIP_FROM_TIP(p)  CONTAINING_RECORD(p, Tip, tip)
#define TIP_FROM_SINK(p) CONTAINING_RECORD(p, Tip, sink)

static void Suggest(Tip *t)
{
    int i;
    t->ncand = 0;
    t->sel   = 0;
    if (t->len == 0) return;
    for (i = 0; g_words[i] && t->ncand < MAX_CAND; i++)
        if (_wcsnicmp(g_words[i], t->buf, t->len) == 0)
            t->cand[t->ncand++] = g_words[i];
}

/* ----------------------------------------------------------- popup window */
static void PopupMetrics(HDC dc, Tip *t, int *w, int *h)
{
    WCHAR line[96];
    SIZE sz;
    int i, maxw = 120;

    wsprintfW(line, L"> %s", t->buf);
    GetTextExtentPoint32W(dc, line, lstrlenW(line), &sz);
    if (sz.cx > maxw) maxw = sz.cx;
    for (i = 0; i < t->ncand; i++) {
        wsprintfW(line, L"%d. %s", i + 1, t->cand[i]);
        GetTextExtentPoint32W(dc, line, lstrlenW(line), &sz);
        if (sz.cx > maxw) maxw = sz.cx;
    }
    wsprintfW(line, L"before: [%s]", t->before);
    GetTextExtentPoint32W(dc, line, lstrlenW(line), &sz);
    if (sz.cx > maxw) maxw = sz.cx;

    *w = maxw + 16;
    *h = (t->ncand + 2) * 20 + 8;
}

static LRESULT CALLBACK PopupProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Tip *t = (Tip *)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (m) {
    case WM_NCCREATE:
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)l)->lpCreateParams);
        break;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;                    /* never steal focus */
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc, r;
        WCHAR line[96];
        int i, y = 4;
        HGDIOBJ oldf = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

        GetClientRect(h, &rc);
        FillRect(dc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        SetBkMode(dc, TRANSPARENT);
        if (t) {
            /* header: what has been typed */
            r.left = 8; r.right = rc.right - 8; r.top = y; r.bottom = y + 20;
            SetTextColor(dc, RGB(90, 90, 90));
            wsprintfW(line, L"> %s", t->buf);
            DrawTextW(dc, line, -1, &r, DT_SINGLELINE | DT_VCENTER);
            y += 20;

            /* candidates */
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            for (i = 0; i < t->ncand; i++, y += 20) {
                r.left = 2; r.right = rc.right - 2; r.top = y; r.bottom = y + 20;
                if (i == t->sel) {
                    HBRUSH b = CreateSolidBrush(RGB(190, 215, 255));
                    FillRect(dc, &r, b);
                    DeleteObject(b);
                }
                r.left = 8;
                wsprintfW(line, L"%d. %s", i + 1, t->cand[i]);
                DrawTextW(dc, line, -1, &r, DT_SINGLELINE | DT_VCENTER);
            }

            /* footer: text we READ from the document */
            r.left = 8; r.right = rc.right - 8; r.top = y; r.bottom = y + 20;
            SetTextColor(dc, RGB(130, 130, 130));
            wsprintfW(line, L"before: [%s]", t->before);
            DrawTextW(dc, line, -1, &r, DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(dc, oldf);
        EndPaint(h, &ps);
        return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

static void PopupCreate(Tip *t)
{
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.cbSize        = sizeof wc;
    wc.lpfnWndProc   = PopupProc;
    wc.hInstance     = g_hinst;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"SampleCTipPopup";
    RegisterClassExW(&wc);           /* fails harmlessly if already registered */

    t->popup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                               wc.lpszClassName, L"", WS_POPUP | WS_BORDER,
                               0, 0, 10, 10, NULL, NULL, g_hinst, t);
}

static void PopupHide(Tip *t)
{
    if (t->popup) ShowWindow(t->popup, SW_HIDE);
}

static void PopupShow(Tip *t)
{
    HDC dc;
    HGDIOBJ oldf;
    int w, h;

    if (!t->popup) return;
    if (t->len == 0) { PopupHide(t); return; }

    dc   = GetDC(t->popup);
    oldf = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    PopupMetrics(dc, t, &w, &h);
    SelectObject(dc, oldf);
    ReleaseDC(t->popup, dc);

    SetWindowPos(t->popup, HWND_TOPMOST, t->caret.x, t->caret.y + 2, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(t->popup, NULL, TRUE);
}

/* ---------------------------------------------------------- edit session
 * ALL reading/writing of document text must happen inside DoEditSession.
 * We only use synchronous sessions, so the session object lives on the stack.
 */
enum { ACT_UPDATE = 1, ACT_COMMIT = 2 };

typedef struct Session {
    ITfEditSession es;
    LONG           ref;
    Tip           *tip;
    ITfContext    *ctx;
    int            action;
    const WCHAR   *text;             /* for ACT_COMMIT */
} Session;

static HRESULT STDMETHODCALLTYPE Es_QI(ITfEditSession *This, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITfEditSession)) {
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE Es_AddRef(ITfEditSession *This)  { return 2; }
static ULONG STDMETHODCALLTYPE Es_Release(ITfEditSession *This) { return 1; }  /* stack object */

/* Read up to BEFORE_CHARS characters preceding the caret */
static void ReadBeforeCaret(Tip *t, TfEditCookie ec, ITfRange *caret)
{
    ITfRange *r = NULL;
    LONG moved = 0;
    ULONG got = 0;

    t->before[0] = 0;
    if (FAILED(ITfRange_Clone(caret, &r)) || !r) return;
    ITfRange_Collapse(r, ec, TF_ANCHOR_START);
    ITfRange_ShiftStart(r, ec, -BEFORE_CHARS, &moved, NULL);
    if (SUCCEEDED(ITfRange_GetText(r, ec, 0, t->before, BEFORE_CHARS, &got)))
        t->before[got] = 0;
    ITfRange_Release(r);
}

/* Find where to put the popup (screen coordinates) */
static void LocateCaret(Tip *t, TfEditCookie ec, ITfContext *ctx, ITfRange *caret)
{
    ITfContextView *view = NULL;
    RECT rc = {0};
    BOOL clipped = FALSE;
    BOOL ok = FALSE;

    if (SUCCEEDED(ITfContext_GetActiveView(ctx, &view)) && view) {
        if (SUCCEEDED(ITfContextView_GetTextExt(view, ec, caret, &rc, &clipped)) &&
            (rc.right != rc.left || rc.bottom != rc.top)) {
            t->caret.x = rc.left;
            t->caret.y = rc.bottom;
            ok = TRUE;
        }
        ITfContextView_Release(view);
    }
    if (!ok) {                                   /* fallback for apps that return an empty rect */
        GUITHREADINFO gti;
        gti.cbSize = sizeof gti;
        if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
            POINT p = { gti.rcCaret.left, gti.rcCaret.bottom };
            ClientToScreen(gti.hwndCaret, &p);
            t->caret = p;
        }
    }
}

static HRESULT STDMETHODCALLTYPE Es_DoEditSession(ITfEditSession *This, TfEditCookie ec)
{
    Session *s = (Session *)This;
    Tip *t = s->tip;
    ITfContext *ctx = s->ctx;

    if (s->action == ACT_UPDATE) {
        TF_SELECTION sel;
        ULONG fetched = 0;
        if (SUCCEEDED(ITfContext_GetSelection(ctx, ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched))
            && fetched == 1 && sel.range) {
            ReadBeforeCaret(t, ec, sel.range);
            LocateCaret(t, ec, ctx, sel.range);
            ITfRange_Release(sel.range);
        }
    }
    else if (s->action == ACT_COMMIT) {
        ITfInsertAtSelection *ins = NULL;
        if (SUCCEEDED(ITfContext_QueryInterface(ctx, &IID_ITfInsertAtSelection, (void **)&ins)) && ins) {
            ITfRange *r = NULL;
            if (SUCCEEDED(ITfInsertAtSelection_InsertTextAtSelection(
                              ins, ec, 0, s->text, (LONG)wcslen(s->text), &r)) && r) {
                /* move the caret to just after the inserted text */
                TF_SELECTION sel;
                ITfRange_Collapse(r, ec, TF_ANCHOR_END);
                sel.range = r;
                sel.style.ase = TF_AE_NONE;
                sel.style.fInterimChar = FALSE;
                ITfContext_SetSelection(ctx, ec, 1, &sel);
                ITfRange_Release(r);
            }
            ITfInsertAtSelection_Release(ins);
        }
    }
    return S_OK;
}

static ITfEditSessionVtbl g_esVtbl = { Es_QI, Es_AddRef, Es_Release, Es_DoEditSession };

static void RunSession(Tip *t, ITfContext *ctx, int action, const WCHAR *text)
{
    Session s;
    HRESULT hrSession = E_FAIL;
    s.es.lpVtbl = &g_esVtbl;
    s.ref = 1;
    s.tip = t;
    s.ctx = ctx;
    s.action = action;
    s.text = text;
    ITfContext_RequestEditSession(ctx, t->client, &s.es,
                                  TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
}

static void Reset(Tip *t)
{
    t->len = 0;
    t->buf[0] = 0;
    t->ncand = 0;
    t->sel = 0;
    PopupHide(t);
}

static void Commit(Tip *t, ITfContext *ctx, const WCHAR *text)
{
    RunSession(t, ctx, ACT_COMMIT, text);
    Reset(t);
}

/* ------------------------------------------------------- key event sink */
static HRESULT STDMETHODCALLTYPE Ks_QI(ITfKeyEventSink *This, REFIID riid, void **ppv);
static ULONG   STDMETHODCALLTYPE Ks_AddRef(ITfKeyEventSink *This)  { return InterlockedIncrement(&TIP_FROM_SINK(This)->ref); }
static ULONG   STDMETHODCALLTYPE Tip_Release(ITfTextInputProcessor *This);
static ULONG   STDMETHODCALLTYPE Ks_Release(ITfKeyEventSink *This);

static HRESULT STDMETHODCALLTYPE Ks_OnSetFocus(ITfKeyEventSink *This, BOOL fForeground)
{
    if (!fForeground) Reset(TIP_FROM_SINK(This));     /* lost focus: drop the composition */
    return S_OK;
}

static BOOL WantKey(Tip *t, WPARAM vk)
{
    if (GetKeyState(VK_CONTROL) & 0x8000) return FALSE;
    if (GetKeyState(VK_MENU) & 0x8000)    return FALSE;
    if (vk >= 'A' && vk <= 'Z') return TRUE;
    if (t->len > 0) {
        switch (vk) {
        case VK_BACK: case VK_RETURN: case VK_SPACE: case VK_ESCAPE:
        case VK_UP:   case VK_DOWN:
            return TRUE;
        }
        if (vk >= '1' && vk <= '9') return TRUE;
    }
    return FALSE;
}

static HRESULT STDMETHODCALLTYPE Ks_OnTestKeyDown(ITfKeyEventSink *This, ITfContext *pic,
                                                  WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    *pfEaten = WantKey(TIP_FROM_SINK(This), wParam);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Ks_OnKeyDown(ITfKeyEventSink *This, ITfContext *pic,
                                              WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    Tip *t = TIP_FROM_SINK(This);

    *pfEaten = WantKey(t, wParam);
    if (!*pfEaten) return S_OK;

    if (wParam >= 'A' && wParam <= 'Z') {
        BOOL upper = ((GetKeyState(VK_SHIFT) & 0x8000) != 0) ^ ((GetKeyState(VK_CAPITAL) & 1) != 0);
        if (t->len < MAX_BUF - 1) {
            t->buf[t->len++] = (WCHAR)(upper ? wParam : wParam - 'A' + 'a');
            t->buf[t->len] = 0;
        }
        Suggest(t);
        RunSession(t, pic, ACT_UPDATE, NULL);        /* read text + caret position */
        PopupShow(t);
    }
    else switch (wParam) {
    case VK_BACK:
        if (t->len > 0) t->buf[--t->len] = 0;
        Suggest(t);
        if (t->len == 0) Reset(t);
        else PopupShow(t);
        break;
    case VK_ESCAPE:
        Reset(t);
        break;
    case VK_DOWN:
        if (t->ncand) { t->sel = (t->sel + 1) % t->ncand; InvalidateRect(t->popup, NULL, TRUE); }
        break;
    case VK_UP:
        if (t->ncand) { t->sel = (t->sel + t->ncand - 1) % t->ncand; InvalidateRect(t->popup, NULL, TRUE); }
        break;
    case VK_SPACE:
    case VK_RETURN: {
        WCHAR out[MAX_BUF + 2];
        lstrcpynW(out, t->ncand ? t->cand[t->sel] : t->buf, MAX_BUF);
        if (wParam == VK_SPACE) lstrcatW(out, L" ");
        Commit(t, pic, out);
        break; }
    default:
        if (wParam >= '1' && wParam <= '9') {
            int i = (int)(wParam - '1');
            if (i < t->ncand) {
                WCHAR out[MAX_BUF + 2];
                lstrcpynW(out, t->cand[i], MAX_BUF);
                lstrcatW(out, L" ");
                Commit(t, pic, out);
            }
        }
        break;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Ks_OnTestKeyUp(ITfKeyEventSink *This, ITfContext *pic,
                                                WPARAM w, LPARAM l, BOOL *pfEaten)
{ *pfEaten = FALSE; return S_OK; }
static HRESULT STDMETHODCALLTYPE Ks_OnKeyUp(ITfKeyEventSink *This, ITfContext *pic,
                                            WPARAM w, LPARAM l, BOOL *pfEaten)
{ *pfEaten = FALSE; return S_OK; }
static HRESULT STDMETHODCALLTYPE Ks_OnPreservedKey(ITfKeyEventSink *This, ITfContext *pic,
                                                   REFGUID rguid, BOOL *pfEaten)
{ *pfEaten = FALSE; return S_OK; }

static ITfKeyEventSinkVtbl g_ksVtbl = {
    Ks_QI, Ks_AddRef, Ks_Release, Ks_OnSetFocus,
    Ks_OnTestKeyDown, Ks_OnTestKeyUp, Ks_OnKeyDown, Ks_OnKeyUp, Ks_OnPreservedKey
};

/* --------------------------------------------- the text input processor */
static HRESULT STDMETHODCALLTYPE Tip_QI(ITfTextInputProcessor *This, REFIID riid, void **ppv)
{
    Tip *t = TIP_FROM_TIP(This);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITfTextInputProcessor)) {
        *ppv = &t->tip;
    } else if (IsEqualIID(riid, &IID_ITfKeyEventSink)) {
        *ppv = &t->sink;
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    InterlockedIncrement(&t->ref);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE Tip_AddRef(ITfTextInputProcessor *This)
{
    return InterlockedIncrement(&TIP_FROM_TIP(This)->ref);
}

static ULONG STDMETHODCALLTYPE Tip_Release(ITfTextInputProcessor *This)
{
    Tip *t = TIP_FROM_TIP(This);
    LONG r = InterlockedDecrement(&t->ref);
    if (r == 0) {
        HeapFree(GetProcessHeap(), 0, t);
        InterlockedDecrement(&g_dllRefs);
    }
    return (ULONG)r;
}

static ULONG STDMETHODCALLTYPE Ks_Release(ITfKeyEventSink *This)
{
    return Tip_Release(&TIP_FROM_SINK(This)->tip);
}

static HRESULT STDMETHODCALLTYPE Ks_QI(ITfKeyEventSink *This, REFIID riid, void **ppv)
{
    return Tip_QI(&TIP_FROM_SINK(This)->tip, riid, ppv);
}

static HRESULT STDMETHODCALLTYPE Tip_Activate(ITfTextInputProcessor *This, ITfThreadMgr *ptim, TfClientId tid)
{
    Tip *t = TIP_FROM_TIP(This);
    ITfKeystrokeMgr *ksm = NULL;
    HRESULT hr;

    Log(L"tipdemo: Activate\n");
    t->tm = ptim;
    ITfThreadMgr_AddRef(ptim);
    t->client = tid;

    hr = ITfThreadMgr_QueryInterface(ptim, &IID_ITfKeystrokeMgr, (void **)&ksm);
    if (SUCCEEDED(hr)) {
        hr = ITfKeystrokeMgr_AdviseKeyEventSink(ksm, tid, &t->sink, TRUE);
        ITfKeystrokeMgr_Release(ksm);
    }
    if (FAILED(hr)) {
        ITfThreadMgr_Release(t->tm);
        t->tm = NULL;
        return hr;
    }
    PopupCreate(t);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Tip_Deactivate(ITfTextInputProcessor *This)
{
    Tip *t = TIP_FROM_TIP(This);
    ITfKeystrokeMgr *ksm = NULL;

    Log(L"tipdemo: Deactivate\n");
    if (t->tm) {
        if (SUCCEEDED(ITfThreadMgr_QueryInterface(t->tm, &IID_ITfKeystrokeMgr, (void **)&ksm))) {
            ITfKeystrokeMgr_UnadviseKeyEventSink(ksm, t->client);
            ITfKeystrokeMgr_Release(ksm);
        }
        ITfThreadMgr_Release(t->tm);
        t->tm = NULL;
    }
    if (t->popup) { DestroyWindow(t->popup); t->popup = NULL; }
    t->client = TF_CLIENTID_NULL;
    return S_OK;
}

static ITfTextInputProcessorVtbl g_tipVtbl = {
    Tip_QI, Tip_AddRef, Tip_Release, Tip_Activate, Tip_Deactivate
};

/* ---------------------------------------------------------- class factory */
static HRESULT STDMETHODCALLTYPE Cf_QI(IClassFactory *This, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE Cf_AddRef(IClassFactory *This)  { return 2; }
static ULONG STDMETHODCALLTYPE Cf_Release(IClassFactory *This) { return 1; }

static HRESULT STDMETHODCALLTYPE Cf_CreateInstance(IClassFactory *This, IUnknown *outer,
                                                   REFIID riid, void **ppv)
{
    Tip *t;
    HRESULT hr;

    *ppv = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    t = (Tip *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *t);
    if (!t) return E_OUTOFMEMORY;
    t->tip.lpVtbl  = &g_tipVtbl;
    t->sink.lpVtbl = &g_ksVtbl;
    t->ref = 1;
    t->client = TF_CLIENTID_NULL;
    InterlockedIncrement(&g_dllRefs);

    hr = Tip_QI(&t->tip, riid, ppv);
    Tip_Release(&t->tip);                         /* drop our creation ref */
    return hr;
}

static HRESULT STDMETHODCALLTYPE Cf_LockServer(IClassFactory *This, BOOL lock)
{
    if (lock) InterlockedIncrement(&g_dllRefs); else InterlockedDecrement(&g_dllRefs);
    return S_OK;
}

static IClassFactoryVtbl g_cfVtbl = { Cf_QI, Cf_AddRef, Cf_Release, Cf_CreateInstance, Cf_LockServer };
static IClassFactory g_factory = { &g_cfVtbl };

/* ------------------------------------------------------------ DLL exports */
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hinst = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    *ppv = NULL;
    if (!IsEqualCLSID(rclsid, &CLSID_SampleTip)) return CLASS_E_CLASSNOTAVAILABLE;
    return Cf_QI(&g_factory, riid, ppv);
}

STDAPI DllCanUnloadNow(void)
{
    return g_dllRefs == 0 ? S_OK : S_FALSE;
}

/* ------------------------------------------------------------ registration */
static void GuidKey(const GUID *g, WCHAR *out) { StringFromGUID2(g, out, 64); }

STDAPI DllRegisterServer(void)
{
    WCHAR clsid[64], key[160], path[MAX_PATH];
    HRESULT hr, hrInit;
    ITfInputProcessorProfileMgr *mgr = NULL;
    ITfCategoryMgr *cat = NULL;
    LSTATUS st;
    static const WCHAR apt[] = L"Apartment";

    GuidKey(&CLSID_SampleTip, clsid);
    GetModuleFileNameW(g_hinst, path, MAX_PATH);

    /* 1. COM registration */
    wsprintfW(key, L"CLSID\\%s", clsid);
    st = RegSetKeyValueW(HKEY_CLASSES_ROOT, key, NULL, REG_SZ, TIP_DESC, sizeof(TIP_DESC));
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);       /* usually: not elevated */
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid);
    RegSetKeyValueW(HKEY_CLASSES_ROOT, key, NULL, REG_SZ, path, (DWORD)((lstrlenW(path) + 1) * 2));
    RegSetKeyValueW(HKEY_CLASSES_ROOT, key, L"ThreadingModel", REG_SZ, apt, sizeof(apt));

    /* 2. TSF profile (this is what puts it in the keyboard list) */
    hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                          &IID_ITfInputProcessorProfileMgr, (void **)&mgr);
    if (SUCCEEDED(hr)) {
        hr = ITfInputProcessorProfileMgr_RegisterProfile(
                 mgr, &CLSID_SampleTip, TIP_LANGID, &GUID_SampleProfile,
                 TIP_DESC, (ULONG)lstrlenW(TIP_DESC),
                 NULL, 0, 0,          /* no icon */
                 NULL, 0, TRUE, 0);   /* enabled by default */
        ITfInputProcessorProfileMgr_Release(mgr);
    }

    /* 3. Category: mark as a keyboard TIP */
    if (SUCCEEDED(hr) &&
        SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void **)&cat))) {
        ITfCategoryMgr_RegisterCategory(cat, &CLSID_SampleTip,
                                        &GUID_TFCAT_TIP_KEYBOARD, &CLSID_SampleTip);
        ITfCategoryMgr_Release(cat);
    }
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer(void)
{
    WCHAR clsid[64], key[160];
    HRESULT hrInit;
    ITfInputProcessorProfileMgr *mgr = NULL;
    ITfCategoryMgr *cat = NULL;

    hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfCategoryMgr, (void **)&cat))) {
        ITfCategoryMgr_UnregisterCategory(cat, &CLSID_SampleTip,
                                          &GUID_TFCAT_TIP_KEYBOARD, &CLSID_SampleTip);
        ITfCategoryMgr_Release(cat);
    }
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_ITfInputProcessorProfileMgr, (void **)&mgr))) {
        ITfInputProcessorProfileMgr_UnregisterProfile(mgr, &CLSID_SampleTip, TIP_LANGID,
                                                      &GUID_SampleProfile, 0);
        ITfInputProcessorProfileMgr_Release(mgr);
    }
    if (SUCCEEDED(hrInit)) CoUninitialize();

    GuidKey(&CLSID_SampleTip, clsid);
    wsprintfW(key, L"CLSID\\%s\\InprocServer32", clsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    wsprintfW(key, L"CLSID\\%s", clsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    return S_OK;
}