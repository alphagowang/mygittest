
/*-------------------------------------------------------------   加一点

又加了一行  但是发的工行卡啥的感觉都是法国娇兰；是的干净利索的



 * conv_demo.c —— 信号与系统图形化演示
 *
 * 功能:
 *   1. 演示 6 种常见 LTI 系统的单位冲击响应 h[n]
 *   2. 演示 6 种常见输入信号 x[n]
 *   3. 计算并显示卷积输出 y[n] = x[n] * h[n] = Σ x[k]·h[n-k]
 *   4. 动画播放: 直观展示 h[m-k] 翻转平移后与 x[k] 逐点相乘求和
 *
 * 编译 (MinGW-w64 / WinLibs):
 *   gcc conv_demo.c -o conv_demo.exe -O2 -mwindows -lgdi32 -luser32
 *-------------------------------------------------------------*/
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef CLEARTYPE_QUALITY        /* 兼容旧版 MinGW 头文件 */
#define CLEARTYPE_QUALITY 5
#endif

#define N  64             /* x[n] 与 h[n] 的样本数 */
#define NY (2 * N - 1)    /* 卷积输出长度          */
#define PI 3.14159265358979323846

/* ---------------- 控件 ID ---------------- */
#define IDC_SYS   1001
#define IDC_SIG   1002
#define IDC_PLAY  1003
#define IDC_RESET 1004
#define TIMER_STEP 1

/* ---------------- 系统与信号枚举 ---------------- */
enum { SYS_IDENTITY, SYS_DELAY, SYS_MA8, SYS_IIR, SYS_DIFF, SYS_RESON, SYS_N };
enum { SIG_IMP, SIG_STEP, SIG_SIN, SIG_SQR, SIG_EXP, SIG_SAW, SIG_N };

static const wchar_t *SYS_NAME[SYS_N] = {
    L"恒等系统        y[n] = x[n]",
    L"延迟系统        y[n] = x[n-4]",
    L"滑动平均系统    h[n] = 1/8 (0 <= n < 8)",
    L"一阶IIR低通     h[n] = 0.85^n · u[n]",
    L"差分系统        y[n] = x[n] - x[n-1]",
    L"二阶谐振器(带通) r=0.92, ω0=π/8",
};
static const wchar_t *SIG_NAME[SIG_N] = {
    L"单位冲击 δ[n]",
    L"单位阶跃 u[n]",
    L"正弦波 sin(2πn/16)",
    L"方波 (周期16)",
    L"指数衰减 e^(-n/10)",
    L"锯齿波 (周期16)",
};

/* ---------------- 全局数据 ---------------- */
static double h[N], x[N], y[NY], ov[N];   /* 冲击响应 / 输入 / 输出 / 翻转平移后的h */
static int gSys = SYS_IIR;                /* 当前系统 */
static int gSig = SIG_IMP;                /* 当前信号 */
static int step = 0;                      /* 动画当前时刻 m */
static int playing = 0;                   /* 是否正在播放动画 */

static HWND hwndMain, hComboSys, hComboSig, hBtnPlay, hBtnReset;
static HFONT hFontUI, hFontTitle, hFontInfo, hFontSmall;

/* ---------------- 颜色 ---------------- */
#define CLR_X      RGB(0, 90, 200)     /* 输入 x   : 蓝 */
#define CLR_H      RGB(0, 140, 70)     /* 冲击响应 : 绿 */
#define CLR_Y      RGB(200, 30, 30)    /* 输出 y   : 红 */
#define CLR_OV     RGB(235, 140, 0)    /* 翻转平移 : 橙 */
#define CLR_GRID   RGB(215, 215, 215)
#define CLR_DIM    RGB(210, 210, 210)
#define CLR_TEXT   RGB(60, 60, 60)

/* ================= 序列生成 ================= */
static void BuildH(void)
{
    memset(h, 0, sizeof(h));
    switch (gSys) {
    case SYS_IDENTITY:                       /* 恒等: h[n]=δ[n] */
        h[0] = 1; break;
    case SYS_DELAY:                          /* 延迟4: h[n]=δ[n-4] */
        h[4] = 1; break;
    case SYS_MA8:                            /* 8点滑动平均 */
        for (int i = 0; i < 8; i++) h[i] = 1.0 / 8.0;
        break;
    case SYS_IIR:                            /* 一阶IIR低通: 0.85^n */
        for (int i = 0; i < N; i++) h[i] = pow(0.85, i);
        break;
    case SYS_DIFF:                           /* 差分: δ[n]-δ[n-1] */
        h[0] = 1; h[1] = -1; break;
    case SYS_RESON:                          /* 二阶谐振器(共轭复极点) */
    {
        double r = 0.92, w0 = PI / 8.0;
        for (int i = 0; i < N; i++)
            h[i] = pow(r, i) * sin(w0 * (i + 1)) / sin(w0);
        break;
    }
    }
}

static void BuildX(void)
{
    memset(x, 0, sizeof(x));
    switch (gSig) {
    case SIG_IMP:  x[0] = 1; break;          /* δ[n] */
    case SIG_STEP: for (int i = 0; i < N; i++) x[i] = 1; break;   /* u[n] */
    case SIG_SIN:  for (int i = 0; i < N; i++) x[i] = sin(2 * PI * i / 16.0); break;
    case SIG_SQR:  for (int i = 0; i < N; i++) x[i] = ((i % 16) < 8) ? 1 : -1; break;
    case SIG_EXP:  for (int i = 0; i < N; i++) x[i] = exp(-i / 10.0); break;
    case SIG_SAW:  for (int i = 0; i < N; i++) x[i] = 2.0 * (i % 16) / 15.0 - 1; break;
    }
}

/* 卷积 y[n] = Σ x[k]·h[n-k] */
static void DoConv(void)
{
    for (int n = 0; n < NY; n++) {
        double s = 0;
        for (int k = 0; k < N; k++) {
            int m = n - k;
            if (m >= 0 && m < N) s += x[k] * h[m];
        }
        y[n] = s;
    }
}

/* 当前动画时刻 m: 求翻转平移后的 h[m-k] (与 x 对齐画在同一面板) */
static void BuildOverlay(void)
{
    for (int k = 0; k < N; k++)
        ov[k] = (step - k >= 0 && step - k < N) ? h[step - k] : 0.0;
}

static void Recompute(void)
{
    BuildH(); BuildX(); DoConv(); BuildOverlay();
}

/* ================= 绘制 ================= */

/* 画一个面板: 标题 / 主序列(progress前实色, 之后灰色) / 叠加序列 / 高亮 */
static void DrawPanel(HDC dc, RECT r, const wchar_t *title, COLORREF tclr,
                      const double *d, int len, COLORREF clr,
                      int progress, int hi, int showVal,
                      const double *ovl, COLORREF ovc)
{
    HBRUSH bg = CreateSolidBrush(RGB(252, 252, 253));
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    FrameRect(dc, &r, (HBRUSH)GetStockObject(GRAY_BRUSH));

    int left = r.left + 10, right = r.right - 10;
    int top = r.top + 24, bot = r.bottom - 18;
    int cy = (top + bot) / 2;

    /* 幅度归一化 */
    double maxabs = 1e-9;
    for (int i = 0; i < len; i++) { double a = fabs(d[i]);   if (a > maxabs) maxabs = a; }
    if (ovl) for (int i = 0; i < len; i++) { double a = fabs(ovl[i]); if (a > maxabs) maxabs = a; }
    double scale = ((bot - top) / 2.0 - 6.0) / maxabs;
    double dx = (double)(right - left) / (len - 1);

    /* 网格 */
    HPEN gp = CreatePen(PS_DOT, 1, CLR_GRID);
    HGDIOBJ oo = SelectObject(dc, gp);
    for (int i = 0; i < len; i += 8) {
        int px = left + (int)(i * dx + 0.5);
        MoveToEx(dc, px, top, NULL); LineTo(dc, px, bot);
    }
    SelectObject(dc, oo); DeleteObject(gp);

    /* 高亮: 当前翻转中心虚线 */
    if (hi >= 0 && hi < len) {
        HPEN dp = CreatePen(PS_DASH, 1, ovl ? ovc : tclr);
        oo = SelectObject(dc, dp);
        int px = left + (int)(hi * dx + 0.5);
        MoveToEx(dc, px, top, NULL); LineTo(dc, px, bot);
        SelectObject(dc, oo); DeleteObject(dp);
    }

    /* 叠加序列 (翻转平移的 h[m-k]) */
    if (ovl) {
        HPEN p = CreatePen(PS_SOLID, 1, ovc);
        HBRUSH b = CreateSolidBrush(ovc);
        HGDIOBJ po = SelectObject(dc, p), bo = SelectObject(dc, b);
        for (int i = 0; i < len; i++) {
            if (ovl[i] == 0) continue;
            int px = left + (int)(i * dx + 0.5);
            int py = cy - (int)(ovl[i] * scale);
            MoveToEx(dc, px, cy, NULL); LineTo(dc, px, py);
            Ellipse(dc, px - 3, py - 3, px + 3, py + 3);
        }
        SelectObject(dc, po); SelectObject(dc, bo);
        DeleteObject(p); DeleteObject(b);
    }

    /* 主序列 (棒图 stem) */
    for (int i = 0; i < len; i++) {
        COLORREF c = (progress >= 0 && i > progress) ? CLR_DIM : clr;
        int px = left + (int)(i * dx + 0.5);
        int py = cy - (int)(d[i] * scale);
        HPEN p = CreatePen(PS_SOLID, 1, c);
        HBRUSH b = CreateSolidBrush(c);
        HGDIOBJ po = SelectObject(dc, p), bo = SelectObject(dc, b);
        MoveToEx(dc, px, cy, NULL); LineTo(dc, px, py);
        if (i == hi) Ellipse(dc, px - 5, py - 5, px + 5, py + 5);
        else         Ellipse(dc, px - 2, py - 2, px + 2, py + 2);
        SelectObject(dc, po); SelectObject(dc, bo);
        DeleteObject(p); DeleteObject(b);

        /* 高亮点数值标注 */
        if (i == hi && showVal) {
            wchar_t vb[32];
            _snwprintf(vb, 32, L"%.3f", d[i]);
            SetTextColor(dc, clr);
            SetBkMode(dc, TRANSPARENT);
            int ty = (d[i] >= 0) ? py - 26 : py + 8;
            if (ty < top) ty = top;
            if (ty > bot - 16) ty = bot - 16;
            RECT tr = { px - 44, ty, px + 44, ty + 16 };
            DrawTextW(dc, vb, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    /* 零线 */
    HPEN zp = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    oo = SelectObject(dc, zp);
    MoveToEx(dc, left, cy, NULL); LineTo(dc, right, cy);
    SelectObject(dc, oo); DeleteObject(zp);

    /* 横轴刻度数字 */
    HGDIOBJ fo = SelectObject(dc, hFontSmall);
    SetTextColor(dc, RGB(140, 140, 140));
    SetBkMode(dc, TRANSPARENT);
    wchar_t tb[8];
    for (int i = 0; i < len; i += 16) {
        _snwprintf(tb, 8, L"%d", i);
        int px = left + (int)(i * dx + 0.5);
        TextOutW(dc, px - 4, bot + 3, tb, (int)wcslen(tb));
    }
    SelectObject(dc, fo);

    /* 标题 */
    HGDIOBJ to = SelectObject(dc, hFontTitle);
    SetTextColor(dc, tclr);
    TextOutW(dc, r.left + 8, r.top + 3, title, (int)wcslen(title));
    SelectObject(dc, to);
}

static void OnPaint(HDC wdc)
{
    RECT rc; GetClientRect(hwndMain, &rc);

    /* 双缓冲 */
    static HDC mdc = NULL; static HBITMAP mbm = NULL;
    static int mw = 0, mh = 0;
    if (!mdc || mw != rc.right || mh != rc.bottom) {
        if (mbm) DeleteObject(mbm);
        if (mdc) DeleteDC(mdc);
        mdc = CreateCompatibleDC(wdc);
        mbm = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
        SelectObject(mdc, mbm);
        mw = rc.right; mh = rc.bottom;
    }
    FillRect(mdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    BuildOverlay();   /* 动画推进时随 step 更新 */

    int mg = 12, gap = 8, py0 = 56, infoH = 32;
    int W = rc.right - 2 * mg;
    int ph = (rc.bottom - py0 - infoH - 2 * gap) / 3;
    RECT r1 = { mg, py0, mg + W, py0 + ph };
    RECT r2 = { mg, py0 + ph + gap, mg + W, py0 + 2 * ph + gap };
    RECT r3 = { mg, py0 + 2 * (ph + gap), mg + W, py0 + 3 * ph + 2 * gap };
    RECT ri = { 0, rc.bottom - infoH, rc.right, rc.bottom };

    int anim = (playing || step > 0);
    int hiX = (anim && step < N) ? step : -1;
    int hiY = anim ? step : -1;

    DrawPanel(mdc, r1, L"输入信号 x[n]    (橙色 = 翻转平移后的 h[m-k])",
              CLR_X, x, N, CLR_X, -1, hiX, 0, anim ? ov : NULL, CLR_OV);
    DrawPanel(mdc, r2, L"单位冲击响应 h[n]", CLR_H, h, N, CLR_H, -1, -1, 0, NULL, 0);
    DrawPanel(mdc, r3, L"输出  y[n] = x[n] * h[n]", CLR_Y, y, NY, CLR_Y,
              anim ? step : -1, hiY, anim, NULL, 0);

    /* 底部信息栏 */
    HBRUSH ib = CreateSolidBrush(RGB(245, 246, 248));
    FillRect(mdc, &ri, ib);
    DeleteObject(ib);
    HGDIOBJ fo = SelectObject(mdc, hFontInfo);
    SetBkMode(mdc, TRANSPARENT);
    SetTextColor(mdc, CLR_TEXT);
    wchar_t msg[200];
    if (anim) {
        _snwprintf(msg, 200,
                 L"卷积动画  y[%d] = Σ x[k]·h[%d-k] = %.4f      (第 %d / %d 步)",
                 step, step, y[step], step + 1, NY);
    } else {
        _snwprintf(msg, 200,
                 L"y[n] = x[n]*h[n] = Σ x[k]·h[n-k]        点击【播放动画】观看卷积滑动过程");
    }
    TextOutW(mdc, 14, ri.top + 6, msg, (int)wcslen(msg));
    SelectObject(mdc, fo);

    BitBlt(wdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
}

/* ================= 交互 ================= */
static void StopAnim(void)
{
    if (playing) { KillTimer(hwndMain, TIMER_STEP); playing = 0; }
    SetWindowTextW(hBtnPlay, L"播放动画");
}

static void ResetAll(void)
{
    StopAnim();
    step = 0;
    Recompute();
    InvalidateRect(hwndMain, NULL, FALSE);
}

static void TogglePlay(void)
{
    if (playing) {
        StopAnim();
    } else {
        if (step >= NY - 1) step = 0;      /* 播完则从头再来 */
        playing = 1;
        SetTimer(hwndMain, TIMER_STEP, 60, NULL);
        SetWindowTextW(hBtnPlay, L"暂停动画");
    }
    InvalidateRect(hwndMain, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        hFontUI    = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei");
        hFontTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei");
        hFontInfo  = CreateFontW(-17, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei");
        hFontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei");

        HWND lb = CreateWindowW(L"STATIC", L"系统:", WS_CHILD | WS_VISIBLE,
                                12, 14, 40, 20, hwnd, NULL, NULL, NULL);
        hComboSys = CreateWindowW(L"COMBOBOX", NULL,
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                                  54, 10, 340, 300, hwnd, (HMENU)(INT_PTR)IDC_SYS, NULL, NULL);
        lb = CreateWindowW(L"STATIC", L"信号:", WS_CHILD | WS_VISIBLE,
                           406, 14, 40, 20, hwnd, NULL, NULL, NULL);
        hComboSig = CreateWindowW(L"COMBOBOX", NULL,
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                                  448, 10, 300, 300, hwnd, (HMENU)(INT_PTR)IDC_SIG, NULL, NULL);
        hBtnReset = CreateWindowW(L"BUTTON", L"重置",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  762, 9, 64, 26, hwnd, (HMENU)(INT_PTR)IDC_RESET, NULL, NULL);
        hBtnPlay  = CreateWindowW(L"BUTTON", L"播放动画",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  834, 9, 92, 26, hwnd, (HMENU)(INT_PTR)IDC_PLAY, NULL, NULL);

        SendMessageW(lb, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessageW(hComboSys, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessageW(hComboSig, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessageW(hBtnPlay, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessageW(hBtnReset, WM_SETFONT, (WPARAM)hFontUI, TRUE);

        for (int i = 0; i < SYS_N; i++)
            SendMessageW(hComboSys, CB_ADDSTRING, 0, (LPARAM)SYS_NAME[i]);
        for (int i = 0; i < SIG_N; i++)
            SendMessageW(hComboSig, CB_ADDSTRING, 0, (LPARAM)SIG_NAME[i]);
        SendMessageW(hComboSys, CB_SETCURSEL, gSys, 0);
        SendMessageW(hComboSig, CB_SETCURSEL, gSig, 0);

        Recompute();
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SYS:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int s = (int)SendMessageW(hComboSys, CB_GETCURSEL, 0, 0);
                if (s >= 0 && s != gSys) { gSys = s; ResetAll(); }
            }
            break;
        case IDC_SIG:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int s = (int)SendMessageW(hComboSig, CB_GETCURSEL, 0, 0);
                if (s >= 0 && s != gSig) { gSig = s; ResetAll(); }
            }
            break;
        case IDC_PLAY:  TogglePlay(); break;
        case IDC_RESET: ResetAll();   break;
        }
        break;

    case WM_TIMER:
        if (wp == TIMER_STEP) {
            step++;
            if (step >= NY - 1) {          /* 播放结束 */
                KillTimer(hwndMain, TIMER_STEP);
                playing = 0;
                SetWindowTextW(hBtnPlay, L"重新播放");
            }
            InvalidateRect(hwndMain, NULL, FALSE);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        OnPaint(wdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;                          /* 双缓冲, 防闪烁 */

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hPrev; (void)cmd;

    WNDCLASSW wc = {0};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"ConvDemoWnd";
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT wr = { 0, 0, 1020, 706 };
    AdjustWindowRect(&wr, style, FALSE);

    hwndMain = CreateWindowW(wc.lpszClassName,
                             L"信号与系统演示 —— 常见系统的单位冲击响应与卷积",
                             style, CW_USEDEFAULT, CW_USEDEFAULT,
                             wr.right - wr.left, wr.bottom - wr.top,
                             NULL, NULL, hInst, NULL);
    if (!hwndMain) return 1;

    ShowWindow(hwndMain, show);
    UpdateWindow(hwndMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
