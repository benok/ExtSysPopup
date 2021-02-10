#pragma once

//---------------------------------------------------------------------------//
//
// Settings.hpp
//  設定ファイル 管理クラス
//   Copyright (C) 2016 tapetums
//
//---------------------------------------------------------------------------//

#include <windows.h>

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib") // PathRenameExtension

//---------------------------------------------------------------------------//
// Global Variables
//---------------------------------------------------------------------------//

// WinMain.cpp で宣言
extern HINSTANCE g_hInst;

//---------------------------------------------------------------------------//
// Class
//---------------------------------------------------------------------------//

class Settings
{
public:
    UINT mode;

    INT32 x;
    INT32 y;
    INT32 w;
    INT32 h;

    UINT title;
    UINT min;
    UINT max;
    UINT close;

public:
    Settings()  { load(); }
    ~Settings() { save(); }

public:
    void load();
    void save();
};

extern Settings* settings;

//---------------------------------------------------------------------------//
// Methods
//---------------------------------------------------------------------------//

inline void Settings::load()
{
    wchar_t path [MAX_PATH];

    // iniファイル名取得
    ::GetModuleFileNameW(g_hInst, path, MAX_PATH);
    ::PathRenameExtensionW(path, L".ini");

    // パラメータの取得
    mode = ::GetPrivateProfileIntW(L"Setting", L"mode", 0, path);

    x = ::GetPrivateProfileIntW(L"Setting", L"x",   -32, path);
    y = ::GetPrivateProfileIntW(L"Setting", L"y",     0, path);
    w = ::GetPrivateProfileIntW(L"Setting", L"w",    32, path);
    h = ::GetPrivateProfileIntW(L"Setting", L"h",    32, path);

    title = ::GetPrivateProfileIntW(L"Setting", L"title", 0, path);
    min   = ::GetPrivateProfileIntW(L"Setting", L"min",   0, path);
    max   = ::GetPrivateProfileIntW(L"Setting", L"max",   0, path);
    close = ::GetPrivateProfileIntW(L"Setting", L"close", 0, path);
}

//---------------------------------------------------------------------------//

inline void Settings::save()
{
    wchar_t path [MAX_PATH];
    wchar_t buf  [16];

    // iniファイル名取得
    ::GetModuleFileNameW(g_hInst, path, MAX_PATH);
    ::PathRenameExtensionW(path, L".ini");

    // パラメータの書き出し
    ::wsprintfW(buf, L"%i", mode);
    ::WritePrivateProfileStringW(L"Setting", L"mode", buf, path);

    ::wsprintfW(buf, L"%i", x);
    ::WritePrivateProfileStringW(L"Setting", L"x", buf, path);

    ::wsprintfW(buf, L"%i", y);
    ::WritePrivateProfileStringW(L"Setting", L"y", buf, path);

    ::wsprintfW(buf, L"%i", w);
    ::WritePrivateProfileStringW(L"Setting", L"w", buf, path);

    ::wsprintfW(buf, L"%i", h);
    ::WritePrivateProfileStringW(L"Setting", L"h", buf, path);

    ::wsprintfW(buf, L"%i", title);
    ::WritePrivateProfileStringW(L"Setting", L"title", buf, path);

    ::wsprintfW(buf, L"%i", max);
    ::WritePrivateProfileStringW(L"Setting", L"max", buf, path);

    ::wsprintfW(buf, L"%i", min);
    ::WritePrivateProfileStringW(L"Setting", L"min", buf, path);

    ::wsprintfW(buf, L"%i", close);
    ::WritePrivateProfileStringW(L"Setting", L"close", buf, path);
}

//---------------------------------------------------------------------------//

// Settings.hpp