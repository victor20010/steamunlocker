// Injector.cpp
#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <shlobj.h> // For SHGetFolderPathA
#include <limits>  // For numeric_limits

// DLL文件名（必须与编译出的DLL名称一致，且放在同一目录）
const char* DLL_NAME = "Hook.dll";
// 用于在程序和DLL之间传递AppID的临时文件名
const char* TEMP_APPID_FILE = "steam_appids.tmp";

// 将用户输入的AppID列表写入一个DLL可以读取的临时文件
bool WriteAppIDsToFile(const std::vector<int>& appIds) {
    char tempPath[MAX_PATH];
    // 获取 %LOCALAPPDATA% 目录，这是一个可靠的临时文件位置
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, tempPath))) {
        std::cerr << "错误: 无法获取临时文件夹路径。" << std::endl;
        return false;
    }

    std::string fullPath = std::string(tempPath) + "\\" + TEMP_APPID_FILE;
    std::ofstream outFile(fullPath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        std::cerr << "错误: 无法在 " << fullPath << " 创建临时AppID文件。" << std::endl;
        return false;
    }

    for (int id : appIds) {
        outFile << id << std::endl;
    }
    outFile.close();

    std::cout << "AppID列表已写入: " << fullPath << std::endl;
    return true;
}

DWORD FindSteamProcess() {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (_stricmp(processEntry.szExeFile, "steam.exe") == 0) {
                pid = processEntry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }
    CloseHandle(snapshot);
    return pid;
}

bool KillSteamProcess() {
    DWORD pid = FindSteamProcess();
    if (pid == 0) return true; // Steam未运行

    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!process) return false;

    bool result = TerminateProcess(process, 0);
    CloseHandle(process);
    Sleep(3000); // 等待进程完全终止
    return result;
}

bool InjectDLL(DWORD processId, const char* dllPath) {
    char fullDllPath[MAX_PATH];
    if (GetFullPathNameA(dllPath, MAX_PATH, fullDllPath, NULL) == 0) {
        std::cerr << "错误: 无法获取DLL的完整路径: " << GetLastError() << std::endl;
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!process) {
        std::cerr << "错误: 无法打开目标进程. 错误码: " << GetLastError() << std::endl;
        return false;
    }

    LPVOID remotePath = VirtualAllocEx(process, NULL, strlen(fullDllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!remotePath) {
        std::cerr << "错误: 无法在目标进程中分配内存. 错误码: " << GetLastError() << std::endl;
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, remotePath, fullDllPath, strlen(fullDllPath) + 1, NULL)) {
        std::cerr << "错误: 无法向目标进程写入内存. 错误码: " << GetLastError() << std::endl;
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, remotePath, 0, NULL);
    if (!thread) {
        std::cerr << "错误: 无法创建远程线程. 错误码: " << GetLastError() << std::endl;
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD exitCode;
    GetExitCodeThread(thread, &exitCode);

    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    if (exitCode == 0) {
        std::cerr << "错误: 远程线程执行LoadLibraryA失败。请检查 '" << dllPath << "' 是否有效且与Steam（32位）兼容。" << std::endl;
        return false;
    }

    return true;
}

std::string GetSteamPath() {
    HKEY key;
    char steamPath[MAX_PATH];
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD size = sizeof(steamPath);
        if (RegQueryValueExA(key, "SteamExe", NULL, NULL, (BYTE*)steamPath, &size) == ERROR_SUCCESS) {
            RegCloseKey(key);
            return steamPath;
        }
        RegCloseKey(key);
    }
    return "C:\\Program Files (x86)\\Steam\\steam.exe"; // 默认路径
}

bool StartSteamAndInject(const char* dllPath) {
    std::string steamPath = GetSteamPath();
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(NULL, (LPSTR)steamPath.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cerr << "无法启动Steam. 错误码: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "Steam已在挂起模式下启动 (PID: " << pi.dwProcessId << ")" << std::endl;
    std::cout << "正在注入 " << dllPath << "..." << std::endl;

    bool success = InjectDLL(pi.dwProcessId, dllPath);
    
    std::cout << "恢复Steam进程..." << std::endl;
    ResumeThread(pi.hThread);
    
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    
    return success;
}

int main() {
    std::cout << "Steam AppID 模拟器 (MinHook 修复版)" << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    
    std::vector<int> appIds;
    std::cout << "请输入要模拟拥有的游戏AppID (输入0结束):" << std::endl;
    int appId;
    while (true) {
        std::cout << "AppID: ";
        std::cin >> appId;
        if (std::cin.fail()) {
            std::cout << "无效输入，请输入一个数字。" << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (appId == 0) break;
        if (appId > 0) {
            appIds.push_back(appId);
        }
    }
    
    if (appIds.empty()) {
        std::cout << "未输入任何AppID，程序退出。" << std::endl;
        return 0;
    }
    
    std::cout << "共添加 " << appIds.size() << " 个游戏。" << std::endl;
    
    if (!WriteAppIDsToFile(appIds)) {
        std::cin.get();
        return 1;
    }
    
    std::cout << "正在关闭现有Steam进程..." << std::endl;
    if (!KillSteamProcess()) {
        std::cout << "警告: 无法自动关闭Steam进程，可能需要手动关闭。" << std::endl;
    } else {
        std::cout << "Steam已关闭。" << std::endl;
    }
    
    std::cout << "正在启动Steam并注入钩子..." << std::endl;
    if (StartSteamAndInject(DLL_NAME)) {
        std::cout << "\n操作成功！Steam已启动并注入了钩子。" << std::endl;
        std::cout << "现在你可以尝试从Steam下载或运行这些游戏了。" << std::endl;
    } else {
        std::cout << "\n注入失败！请确保 " << DLL_NAME << " 与本程序在同一目录下，并以管理员权限运行本程序。" << std::endl;
    }
    
    std::cout << "\n按任意键退出..." << std::endl;
    std::cin.ignore();
    std::cin.get();
    
    return 0;
}