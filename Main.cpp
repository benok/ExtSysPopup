﻿//---------------------------------------------------------------------------//
//
// Main.cpp
//
//---------------------------------------------------------------------------//

#include <windows.h>

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include "Plugin.hpp"
#include "MessageDef.hpp"
#include "Utility.hpp"
#include "Settings.hpp"
#include "MouseHook.hpp"

#include "Main.hpp"

//---------------------------------------------------------------------------//
//
// 定数
//
//---------------------------------------------------------------------------//

// プロセスの優先度
constexpr DWORD priority_sheet[] =
{
    REALTIME_PRIORITY_CLASS,
    HIGH_PRIORITY_CLASS,
    ABOVE_NORMAL_PRIORITY_CLASS,
    NORMAL_PRIORITY_CLASS,
    BELOW_NORMAL_PRIORITY_CLASS,
    IDLE_PRIORITY_CLASS,
};

//---------------------------------------------------------------------------//
//
// グローバル変数
//
//---------------------------------------------------------------------------//

HINSTANCE g_hInst  { nullptr };
HANDLE    g_hMutex { nullptr };
HWND      g_hwnd   { nullptr };

Settings* settings { nullptr };

//---------------------------------------------------------------------------//

// コマンドID
enum CMD : INT32
{
    CMD_SHOWPOPUP,
    CMD_COUNT
};

//---------------------------------------------------------------------------//

// プラグインの名前
LPCTSTR PLUGIN_NAME { TEXT("ExtSysPopup") };

// コマンドの数
DWORD COMMAND_COUNT { CMD_COUNT };

//---------------------------------------------------------------------------//

// コマンドの情報
PLUGIN_COMMAND_INFO g_cmd_info[] =
{
    {
        TEXT("Show ExtSysPopup"),           // コマンド名（英名）
        TEXT("ポップアップメニューを表示"), // コマンド説明（日本語）
        CMD_SHOWPOPUP,                      // コマンドID
        0,                                  // Attr（未使用）
        -1,                                 // ResTd(未使用）
        dmHotKeyMenu,                       // DispMenu
        0,                                  // TimerInterval[msec] 0で使用しない
        0                                   // TimerCounter（未使用）
    },
};

//---------------------------------------------------------------------------//

// プラグインの情報
PLUGIN_INFO g_info =
{
    0,                   // プラグインI/F要求バージョン
    (LPTSTR)PLUGIN_NAME, // プラグインの名前（任意の文字が使用可能）
    nullptr,             // プラグインのファイル名（相対パス）
    ptAlwaysLoad,        // プラグインのタイプ
    0,                   // バージョン
    0,                   // バージョン
    COMMAND_COUNT,       // コマンド個数
    &g_cmd_info[0],      // コマンド
    0,                   // ロードにかかった時間（msec）
};

//---------------------------------------------------------------------------//

void LogHWND(HWND hwnd, char* context_str)
{
	int text_len = GetWindowTextLength(hwnd);
	LPSTR win_text = (LPSTR)malloc(text_len + 2);
	GetWindowText(hwnd, win_text, text_len + 2);
	if (win_text) {
		WriteLog(elDebug, TEXT("%s:on %s [%x]%s"), PLUGIN_NAME, context_str, hwnd, win_text);
		free(win_text);
	}
}

bool IsTopMost(HWND hwnd)
{
	// 常に手前かどうかを取得
	const auto styleEx = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	const auto topmost = (styleEx & WS_EX_TOPMOST) ? TRUE : FALSE;

	return topmost;
}

void CheckTopMost(HWND hwnd, HMENU hMenu)
{
    // 常に手前かどうかを取得
    const auto styleEx = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    const auto topmost = (styleEx & WS_EX_TOPMOST) ? TRUE : FALSE;

    // チェックマークを付ける
    MENUITEMINFO mii;
    ::GetMenuItemInfo(hMenu, 0, TRUE, &mii);
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_STATE;
    mii.fState = topmost ? MFS_CHECKED : MFS_UNCHECKED;
    ::SetMenuItemInfo(hMenu, 0, TRUE, &mii);
}

//---------------------------------------------------------------------------//

void ToggleTopMost(HWND hwnd)
{
	LogHWND(hwnd, "ToggleTopMost");

    // 常に手前かどうかを取得
    const auto topmost = IsTopMost(hwnd);

	bool rc;

    // 現在の状態から逆にする
	if (topmost)
	{
		// 「常に手前」を解除
		rc = ::SetWindowPos
		(
			hwnd, HWND_NOTOPMOST,
			0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
		);
		WriteLog(elDebug, TEXT("SetWindowPos(%x, HWND_NOTOPMOST,..) returns %d"), hwnd, rc);
		rc = IsTopMost(hwnd);
		WriteLog(elDebug, TEXT("IsTopMost(%x)=%d"), hwnd, rc);
		if (!rc)
		{
			SetOpaque(hwnd, 255);
		}
		else
		{
			WriteLog(elError, "[hwnd:%x] Failed to unset TOPMOST", hwnd);
		}
    }
    else
    {
        // 「常に手前」を設定
        rc = ::SetWindowPos
        (
            hwnd, HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
		WriteLog(elDebug, TEXT("SetWindowPos(%x, HWND_TOPMOST,..) returns %d"), hwnd, rc);
		rc = IsTopMost(hwnd);
		WriteLog(elDebug, TEXT("IsTopMost(%x)=%d"), hwnd, rc);
		if (rc)
		{
			SetOpaque(hwnd, 230);
			::SetForegroundWindow(hwnd);
		}
		else 
		{
			WriteLog(elError, "[hwnd:%x] Failed to set TOPMOST", hwnd);
		}
    }
}

//---------------------------------------------------------------------------//

BOOL OpenAppFolder(HWND hwnd)
{
    WriteLog(elDebug, TEXT("%s: HWND = %p"), PLUGIN_NAME, hwnd);

    TCHAR path[MAX_PATH];

    DWORD pid;
    ::GetWindowThreadProcessId(hwnd, &pid);
    const auto hProcess = ::OpenProcess
    (
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid
    );
    if ( hProcess == nullptr )
    {
        WriteLog(elDebug, TEXT("%s: OpenProcess() failed"), PLUGIN_NAME);
        return FALSE;
    }

    HMODULE hModule;
    DWORD   cbRet;
    if ( ! ::EnumProcessModules(hProcess, &hModule, sizeof(hModule), &cbRet) )
    {
        WriteLog(elDebug, TEXT("%s: EnumProcessModules() failed"), PLUGIN_NAME);
        return FALSE;
    }

    const auto length = ::GetModuleFileNameEx(hProcess, hModule, path, MAX_PATH);
    WriteLog(elDebug, TEXT("%s: %s"), PLUGIN_NAME, path);

    for ( size_t index = length - 1; index > 0; --index )
    {
        if ( path[index] == '\\' )
        {
            path[index] = '\0';
            break;
        }
    }
    WriteLog(elDebug, TEXT("%s: %s"), PLUGIN_NAME, path);

    ::ShellExecute(nullptr, TEXT("open"), path, nullptr, nullptr, SW_SHOWNOACTIVATE);

    return TRUE;
}

//---------------------------------------------------------------------------//

void CheckOpaque(HWND hwnd, HMENU hMenu)
{
    BYTE alpha;
    ::GetLayeredWindowAttributes(hwnd, nullptr, &alpha, nullptr);
    const UINT index = 10 - 10 * (alpha + 1) / 255;
    WriteLog(elDebug, TEXT("%s: alpha = %u"), PLUGIN_NAME, alpha);

    // チェックマークを付ける
    MENUITEMINFO mii;
    ::GetMenuItemInfo(hMenu, index, TRUE, &mii);
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_STATE;
    mii.fState = MFS_CHECKED;
    ::SetMenuItemInfo(hMenu, index, TRUE, &mii);
}

//---------------------------------------------------------------------------//

BOOL SetOpaque(HWND hwnd, BYTE alpha)
{
    auto styleEx = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, styleEx | WS_EX_LAYERED);

    WriteLog(elDebug, TEXT("%s: alpha = %u"), PLUGIN_NAME, alpha);
    ::SetLayeredWindowAttributes
    (
        hwnd, 0, alpha, LWA_ALPHA
    );

    return TRUE;
}

//---------------------------------------------------------------------------//

void CheckPriority(HWND hwnd, HMENU hMenu)
{
    DWORD dwProcessId;
    ::GetWindowThreadProcessId(hwnd, &dwProcessId);

    const auto hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, 0, dwProcessId);
    const auto priority = ::GetPriorityClass(hProcess);
    ::CloseHandle(hProcess);
    WriteLog(elDebug, TEXT("%s: priority = %X"), PLUGIN_NAME, priority);

    UINT index;
    switch ( priority )
    {
        case REALTIME_PRIORITY_CLASS:     { index = 0; break; }
        case HIGH_PRIORITY_CLASS:         { index = 1; break; }
        case ABOVE_NORMAL_PRIORITY_CLASS: { index = 2; break; }
        case BELOW_NORMAL_PRIORITY_CLASS: { index = 4; break; }
        case IDLE_PRIORITY_CLASS:         { index = 5; break; }
        default:                          { index = 3; break; }
    }

    MENUITEMINFO mii;
    ::GetMenuItemInfo(hMenu, index, TRUE, &mii);
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_STATE;
    mii.fState = MFS_CHECKED;
    ::SetMenuItemInfo(hMenu, index, TRUE, &mii);
}

//---------------------------------------------------------------------------//

BOOL SetPriority(HWND hwnd, DWORD priority)
{
    DWORD dwProcessId;
    ::GetWindowThreadProcessId(hwnd, &dwProcessId);

    const auto hProcess = ::OpenProcess(PROCESS_SET_INFORMATION, 0, dwProcessId);
    ::SetPriorityClass(hProcess, priority);
    ::CloseHandle(hProcess);

    WriteLog(elDebug, TEXT("%s: priority = %X"), PLUGIN_NAME, priority);

    return TRUE;
}

//---------------------------------------------------------------------------//

void ShowPopup(HWND hwnd, HWND target_hwnd)
{
	LogHWND(hwnd, "ShowPopup.hwnd");
	LogHWND(hwnd, "ShowPopup.target_hwnd");

    // リソースからメニューを取得
    const auto hMenu    = ::LoadMenu(g_hInst, MAKEINTRESOURCE(100));
    const auto hSubMenu = ::GetSubMenu(hMenu, 0);

    // チェックマークを付ける
    CheckTopMost (target_hwnd, hSubMenu);
    CheckOpaque  (target_hwnd, ::GetSubMenu(hSubMenu, 2));
    CheckPriority(target_hwnd, ::GetSubMenu(hSubMenu, 3));

    // Article ID: Q135788
    // ポップアップメニューから処理を戻すために必要
    ::SetForegroundWindow(hwnd);

    // ポップアップメニューを表示
    POINT pt;
    ::GetCursorPos(&pt);

    const auto CmdID = ::TrackPopupMenu
    (
        hSubMenu, TPM_LEFTALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
        pt.x, pt.y, 0, hwnd, nullptr
    );
    //WriteLog(elDebug, TEXT("%s: CmdId = %d"), PLUGIN_NAME, CmdID);

    // 表示したメニューを破棄
    ::DestroyMenu(hMenu);

    // Article ID: Q135788
    // ポップアップメニューから処理を戻すために必要
    ::PostMessage(hwnd, WM_NULL, 0, 0);

    // コマンドを実行
    if ( CmdID == 40000 )
    {
        ToggleTopMost(target_hwnd);
    }
    else if ( CmdID == 40001 )
    {
        OpenAppFolder(target_hwnd);
    }
    else if ( 40002 <= CmdID && CmdID <= 40005 )
    {
        ExecutePluginCommand(TEXT(":system"), CmdID - 40002);
    }
    else if ( CmdID == 40006 )
    {
        settings->load();
    }
    else if ( 41000 < CmdID && CmdID < 42000 )
    {
        SetOpaque(target_hwnd, BYTE(255 * (CmdID - 41000) / 100));
    }
    else if ( 42000 < CmdID && CmdID < 43000 )
    {
        SetPriority(target_hwnd, priority_sheet[CmdID - 42001]);
    }
}

//---------------------------------------------------------------------------//

LRESULT CALLBACK OnCommand(HWND hwnd, HWND target_hwnd)
{
	LogHWND(hwnd, "OnCommand.hwnd");
	LogHWND(target_hwnd, "OnCommand.target_hwnd");
	
	if (GetKeyState(VK_SHIFT) < 0) {
		WriteLog(elDebug, TEXT("%s: ShowPopup()"), PLUGIN_NAME);
		ShowPopup(hwnd, target_hwnd);
	} else {
		WriteLog(elDebug, TEXT("%s: ToggleTopMost()"), PLUGIN_NAME);
		ToggleTopMost(target_hwnd);
	}
    
    return 0;
}

//---------------------------------------------------------------------------//

// ウィンドウクラスを登録
ATOM Register(LPCTSTR lpszClassName)
{
    WNDCLASSEX wc;
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hInst;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = lpszClassName;
    wc.hIconSm       = nullptr;
    wc.lpfnWndProc   = [](HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp)
    {
        if ( uMsg == WM_COMMAND ) { return OnCommand(hwnd, HWND(lp)); }
        else { return ::DefWindowProc(hwnd, uMsg, wp, lp); }
    };

    return ::RegisterClassEx(&wc);
}

//---------------------------------------------------------------------------//

// TTBEvent_Init() の内部実装
BOOL WINAPI Init(void)
{
    // フックのために二重起動を禁止
    if ( g_hMutex == nullptr )
    {
        g_hMutex = ::CreateMutex(nullptr, TRUE, PLUGIN_NAME);
    }
    if ( g_hMutex == nullptr )
    {
        WriteLog(elError, TEXT("%s: Failed to create mutex"), PLUGIN_NAME);
        return FALSE;
    }
    if ( ::GetLastError() == ERROR_ALREADY_EXISTS )
    {
        WriteLog(elError, TEXT("%s: Already started"), PLUGIN_NAME);
        return FALSE;
    }

    // ウィンドウクラスを登録
    const auto atom = Register(PLUGIN_NAME);
    if ( atom == 0 )
    {
        if ( GetLastError() == 0x582 )
        {
            // 「そのクラスは既にあります。」
        }
        else
        {
            WriteLog(elError, TEXT("%s: RegisterClassEx() failed"), PLUGIN_NAME);
            goto UNLOAD;
        }
    }

    // TrackPopupMenu() のためにウィンドウを生成
    if ( g_hwnd == nullptr )
    {
        g_hwnd = ::CreateWindowEx
        (
            0, PLUGIN_NAME, PLUGIN_NAME, 0,
            100, 100, 320, 240,
            nullptr, nullptr, g_hInst, nullptr
        );
    }
    if ( nullptr == g_hwnd )
    {
        WriteLog(elError, TEXT("%s: CreateWindowEx() failed"), PLUGIN_NAME);
        goto UNLOAD;
    }

    // 設定ファイルの読み込み
    if ( nullptr == settings )
    {
        settings = new Settings;
    }

    // マウスフックを登録
    if ( ! WMBeginHook() )
    {
        WriteLog(elError, TEXT("%s: Failed to begin hook"), PLUGIN_NAME);
        goto UNLOAD;
    }

    WriteLog(elInfo, TEXT("%s: Successfully initialized"), PLUGIN_NAME);

    return TRUE;

UNLOAD:
    WriteLog(elInfo, TEXT("%s: Initialization failed"), PLUGIN_NAME);

    Unload();

    return FALSE;
}

//---------------------------------------------------------------------------//

// TTBEvent_Unload() の内部実装
void WINAPI Unload(void)
{
    // マウスフックを解除
    WMEndHook();

    // 設定ファイルの書き出し
    if ( settings )
    {
        delete settings; settings = nullptr;
    }

    // ウィンドウを破棄
    if ( g_hwnd )
    {
        ::DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    // ミューテックスを削除
    if ( g_hMutex )
    {
        ::ReleaseMutex(g_hMutex);
        ::CloseHandle(g_hMutex);
        g_hMutex = nullptr;
    }

    WriteLog(elInfo, TEXT("%s: Successfully uninitialized"), PLUGIN_NAME);
}

//---------------------------------------------------------------------------//

// TTBEvent_Execute() の内部実装
BOOL WINAPI Execute(INT32 CmdId, HWND hwnd)
{
    //WriteLog(elDebug, TEXT("%s|%d"), g_info.Filename, CmdId);

    // コマンドを実行する
    switch ( CmdId )
    {
        case CMD_SHOWPOPUP:
        {
            POINT pt;
            ::GetCursorPos(&pt);

            // マウスカーソル直下にあるウィンドウのハンドルを取得
            auto hwnd_target = ::WindowFromPoint(pt);
            if ( nullptr == hwnd_target )
            {
                return FALSE;
            }

            // トップレベルのオーナーウィンドウをさがす
            HWND parent = hwnd_target;
            do
            {
                parent = ::GetParent(parent);
                if ( parent )
                {
                    hwnd_target = parent;
                }
            }
            while ( parent );

            ShowPopup(hwnd, hwnd_target);
            return TRUE;
        }
        default:
        {
            return FALSE;
        }
    }
}

//---------------------------------------------------------------------------//

// TTBEvent_WindowsHook() の内部実装
void WINAPI Hook(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(Msg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
}

//---------------------------------------------------------------------------//
//
// CRT を使わないため new/delete を自前で実装
//
//---------------------------------------------------------------------------//

#if defined(_NODEFLIB)

void* __cdecl operator new(size_t size)
{
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
}

void __cdecl operator delete(void* p)
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void __cdecl operator delete(void* p, size_t) // C++14
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void* __cdecl operator new[](size_t size)
{
    return ::HeapAlloc(::GetProcessHeap(), 0, size);
}

void __cdecl operator delete[](void* p)
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

void __cdecl operator delete[](void* p, size_t) // C++14
{
    if ( p != nullptr ) ::HeapFree(::GetProcessHeap(), 0, p);
}

// プログラムサイズを小さくするためにCRTを除外
#pragma comment(linker, "/nodefaultlib:libcmt.lib")
#pragma comment(linker, "/entry:DllMain")

#endif

//---------------------------------------------------------------------------//

// DLL エントリポイント
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
    if ( fdwReason == DLL_PROCESS_ATTACH ) { g_hInst = hInstance; }

    return TRUE;
}

//---------------------------------------------------------------------------//

// Main.cpp