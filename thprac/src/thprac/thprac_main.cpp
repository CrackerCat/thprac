﻿#include "thprac_main.h"
#include "thprac_games.h"
#include "thprac_gui_locale.h"
#include "thprac_launcher_wnd.h"
#include "thprac_launcher_games.h"
#include "thprac_launcher_cfg.h"
#include "thprac_load_exe.h"
#include "thprac_utils.h"
#include "utils/wininternal.h"
#include <Windows.h>
#include <algorithm>
#include <metrohash128.h>
#include <psapi.h>
#include <string>
#include <tlhelp32.h>

namespace THPrac {
enum thprac_prompt_t {
    PR_FAILED,
    PR_INFO_ATTACHED,
    PR_INFO_NO_GAME_FOUND,
    PR_ASK_IF_ATTACH,
    PR_ASK_IF_CONTINUE,
    PR_ASK_USE_VPATCH,
    PR_ERR_NO_GAME_FOUND,
    PR_ERR_ATTACH_FAILED,
    PR_ERR_RUN_FAILED,
};

bool CheckMutex(const char* mutex_name)
{
    auto result = OpenMutexA(SYNCHRONIZE, FALSE, mutex_name);
    if (result) {
        CloseHandle(result);
        return true;
    }
    return false;
}

bool CheckIfAnyGame()
{
    if (CheckMutex("Touhou Koumakyou App") || CheckMutex("Touhou YouYouMu App") || CheckMutex("Touhou 08 App") || CheckMutex("Touhou 10 App") || CheckMutex("Touhou 11 App") || CheckMutex("Touhou 12 App") || CheckMutex("th17 App") || CheckMutex("th18 App") || CheckMutex("th185 App") || CheckMutex("th19 App"))
        return true;
    return false;
}

int MsgBox(const char* title, const char* text, int flag)
{
    auto titleU16 = utf8_to_utf16(title);
    auto textU16 = utf8_to_utf16(text);
    return MessageBoxW(nullptr, textU16.c_str(), titleU16.c_str(), flag);
}

int MsgBoxWnd(const char* title, const char* text, int flag)
{
    auto titleU16 = utf8_to_utf16(title);
    auto textU16 = utf8_to_utf16(text);
    return LauncherWndMsgBox(titleU16.c_str(), textU16.c_str(), flag);
}

bool PromptUser(thprac_prompt_t info, THGameSig* gameSig = nullptr)
{
    switch (info) {
    case THPrac::PR_FAILED:
        return true;
    case THPrac::PR_INFO_ATTACHED:
        MsgBox(S(THPRAC_PR_COMPLETE), S(THPRAC_PR_INFO_ATTACHED), MB_ICONASTERISK | MB_OK | MB_SYSTEMMODAL);
        return true;
    case THPrac::PR_INFO_NO_GAME_FOUND:
        MsgBoxWnd(S(THPRAC_PR_COMPLETE), S(THPRAC_PR_ERR_NO_GAME), MB_ICONASTERISK | MB_OK | MB_SYSTEMMODAL);
        return true;
    case THPrac::PR_ASK_IF_ATTACH: {
        if (!gameSig) {
            return false;
        }
        char gameExeStr[256];
        sprintf_s(gameExeStr, S(THPRAC_PR_ASK_ATTACH), gameSig->idStr);
        if (MsgBox(S(THPRAC_PR_APPLY), gameExeStr, MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL) == IDYES) {
            return true;
        }
        return false;
    }
    case THPrac::PR_ASK_IF_CONTINUE:
        if (MsgBox(S(THPRAC_PR_CONTINUE), S(THPRAC_PR_ASK_CONTINUE), MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL) == IDYES) {
            return true;
        }
        return false;
    case THPrac::PR_ERR_NO_GAME_FOUND:
        MsgBox(S(THPRAC_PR_ERROR), S(THPRAC_PR_ERR_NO_GAME), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        return true;
    case THPrac::PR_ERR_ATTACH_FAILED:
        MsgBox(S(THPRAC_PR_ERROR), S(THPRAC_PR_ERR_ATTACH), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        return true;
    case THPrac::PR_ERR_RUN_FAILED:
        MsgBox(S(THPRAC_PR_ERROR), S(THPRAC_PR_ERR_RUN), MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        return true;
    default:
        break;
    }

    return false;
}

THGameSig* CheckOngoingGame(PROCESSENTRY32W& proc, uintptr_t* base, HANDLE* pOutHandle = nullptr)
{
    // Eliminate impossible process
    if ( wcscmp(L"東方紅魔郷.exe", proc.szExeFile) && wcscmp(L"alcostg.exe", proc.szExeFile)) {
        if (proc.szExeFile[0] != L't' || proc.szExeFile[1] != L'h')
            return nullptr;
        if (proc.szExeFile[2] < L'0' || proc.szExeFile[2] > L'9')
            return nullptr;
        if (proc.szExeFile[3] < L'0' || proc.szExeFile[3] > L'9')
            return nullptr;
        if (proc.szExeFile[4] == '.') {
            if (*(uint64_t*)(&proc.szExeFile[5]) != 0x0000006500780065)
                return nullptr;
        } else if (proc.szExeFile[4] >= L'0' && proc.szExeFile[4] <= L'9') {
            if (proc.szExeFile[5] != '.')
                return nullptr;
            if (*(uint64_t*)(&proc.szExeFile[6]) != 0x0000006500780065)
                return nullptr;
        } else {
            return nullptr;
        }
    }

    return CheckOngoingGameByPID(proc.th32ProcessID, base, pOutHandle);
}

bool CheckIfGameExistEx(THGameSig& gameSig, const wchar_t* name)
{
    MappedFile file(name);
    if (!file.fileMapView)
        return false;

    ExeSig exeSig;
    GetExeInfo(file.fileMapView, file.fileSize, exeSig);
    for (int i = 0; i < 4; ++i) {
        if (exeSig.metroHash[i] != gameSig.exeSig.metroHash[i]) {
            return false;
        }
    }

    return true;
}

bool CheckIfGameExist(THGameSig& gameSig, std::wstring& name)
{
    if (!strcmp(gameSig.idStr, "th06")) {
        if (CheckIfGameExistEx(gameSig, L"東方紅魔郷.exe")) {
            name = L"東方紅魔郷.exe";
            return true;
        }
    }
    std::wstring n = utf8_to_utf16(gameSig.idStr);
    n += L".exe";
    if (CheckIfGameExistEx(gameSig, n.c_str())) {
        name = n;
        return true;
    }
    return false;
}

bool CheckVpatch(THGameSig& gameSig)
{
    auto attr = GetFileAttributesW(gameSig.vPatchStr);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    return true;
}

bool RunGameWithTHPrac(THGameSig& gameSig, const wchar_t* const name, bool withVpatch)
{
    auto isVpatchValid = withVpatch && CheckDLLFunction(gameSig.vPatchStr, "_Initialize@4");

    STARTUPINFOW startup_info;
    PROCESS_INFORMATION proc_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    CreateProcessW(name, nullptr, nullptr, nullptr, false, CREATE_SUSPENDED, nullptr, nullptr, &startup_info, &proc_info);
    uintptr_t base = GetGameModuleBase(proc_info.hProcess);

    if (isVpatchValid) {
        auto vpNameLength = (wcslen(gameSig.vPatchStr) + 1) * sizeof(wchar_t);
        auto pLoadLibrary = ::LoadLibraryW;
        if (auto remoteStr = VirtualAllocEx(proc_info.hProcess, nullptr, vpNameLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)) {
            WriteProcessMemory(proc_info.hProcess, remoteStr, gameSig.vPatchStr, vpNameLength, nullptr);
            if (auto t = CreateRemoteThread(proc_info.hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, remoteStr, 0, nullptr))
                WaitForSingleObject(t, INFINITE);
            VirtualFreeEx(proc_info.hProcess, remoteStr, 0, MEM_RELEASE);
        }
    }

    auto result = (WriteTHPracSig(proc_info.hProcess, base) && THPrac::LoadSelf(proc_info.hProcess));
    if (!result)
        TerminateThread(proc_info.hThread, ERROR_FUNCTION_FAILED);
    else
        ResumeThread(proc_info.hThread);

    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);
    return result;
}

bool FindOngoingGame(bool prompt_if_no_game, bool prompt_if_yes_game)
{
    bool hasPrompted = false;

    if (CheckIfAnyGame()) {
        THGameSig* gameSig = nullptr;
        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(PROCESSENTRY32W);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                uintptr_t base;
                HANDLE hProc = 0;
                if (!(gameSig = CheckOngoingGame(entry, &base, &hProc)))
                    continue;
                
                hasPrompted = true;
                if (prompt_if_yes_game) {
                    if(!PromptUser(PR_ASK_IF_ATTACH, gameSig)) continue;
                }
                if (WriteTHPracSig(hProc, base) && LoadSelf(hProc)) {
                    if (prompt_if_yes_game) {
                        PromptUser(PR_INFO_ATTACHED);
                    }
                    CloseHandle(snapshot);
                    return true;
                } else {
                    PromptUser(PR_ERR_ATTACH_FAILED);
                    CloseHandle(snapshot);
                    return true;
                }
                
            } while (Process32NextW(snapshot, &entry));
        }
    }

    if (prompt_if_no_game && !hasPrompted) {
        PromptUser(PR_INFO_NO_GAME_FOUND);
    }

    return false;
}

bool FindAndRunGame(bool prompt)
{
    std::wstring name;
    for (auto& sig : gGameDefs) {
        if (CheckIfGameExist(sig, name)) {
            if (prompt) {
                char gameExeStr[256];
                sprintf_s(gameExeStr, S(THPRAC_EXISTING_GAME_CONFIRMATION), sig.idStr);
                auto msgBoxResult = MsgBox(S(THPRAC_EXISTING_GAME_CONFIRMATION_TITLE), gameExeStr, MB_YESNOCANCEL | MB_ICONINFORMATION | MB_SYSTEMMODAL);
                if (msgBoxResult == IDNO) {
                    return false;
                } else if (msgBoxResult != IDYES) {
                    return true;
                }
            }
            if (CheckIfAnyGame()) {
                if (!PromptUser(PR_ASK_IF_CONTINUE))
                    return true;
            }

            if (RunGameWithTHPrac(sig, name.c_str())) {
                return true;
            } else {
                PromptUser(PR_FAILED);
                return true;
            }
        }
    }
    return false;
}
}
