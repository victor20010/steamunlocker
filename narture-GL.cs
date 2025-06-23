// 原始函数指针类型定义
typedef bool (*IsOwnedFunction)(void* thisPtr, AppId_t appId);

// 保存原始函数地址
IsOwnedFunction g_originalIsOwnedFunction = nullptr;

// 替代函数实现
bool HookedIsOwnedFunction(void* thisPtr, AppId_t appId) {
    // 检查appId是否在用户选择的游戏列表中
    if (IsInAppIdList(appId)) {
        return true; // 返回已拥有
    }
    
    // 否则调用原始函数
    return g_originalIsOwnedFunction(thisPtr, appId);
}

// 安装钩子
void InstallHook() {
    // 获取Steam客户端中目标函数地址
    void* targetFunction = GetFunctionAddress("ISteamApps::BIsSubscribedApp");
    
    // 保存原始函数前5字节
    BYTE originalBytes[5];
    memcpy(originalBytes, targetFunction, 5);
    
    // 更改内存保护
    DWORD oldProtect;
    VirtualProtect(targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    
    // 写入跳转指令 (E9 + 32位相对偏移)
    BYTE jumpCode[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
    *(DWORD*)(&jumpCode[1]) = (DWORD)HookedIsOwnedFunction - (DWORD)targetFunction - 5;
    memcpy(targetFunction, jumpCode, 5);
    
    // 恢复内存保护
    VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
    
    // 设置原始函数指针
    g_originalIsOwnedFunction = CreateTrampoline(targetFunction, originalBytes);
}




DLL注射：
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

// 用于存储要"解锁"的游戏 AppIDs
std::vector<unsigned int> g_appIds;

// 通过进程名查找进程ID
DWORD GetProcessIdByName(const wchar_t* processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(processEntry);
        
        if (Process32FirstW(snapshot, &processEntry)) {
            do {
                if (_wcsicmp(processEntry.szExeFile, processName) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    
    return processId;
}

// 向进程注入DLL
bool InjectDLL(DWORD processId, const wchar_t* dllPath) {
    bool result = false;
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    
    if (process) {
        // 在目标进程中分配内存用于存储DLL路径
        size_t pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
        LPVOID remotePath = VirtualAllocEx(process, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (remotePath) {
            // 将DLL路径写入目标进程内存
            if (WriteProcessMemory(process, remotePath, dllPath, pathLen, NULL)) {
                // 获取LoadLibraryW的地址
                FARPROC loadLibraryAddr = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
                
                if (loadLibraryAddr) {
                    // 创建远程线程加载DLL
                    HANDLE thread = CreateRemoteThread(process, NULL, 0, 
                        (LPTHREAD_START_ROUTINE)loadLibraryAddr, remotePath, 0, NULL);
                    
                    if (thread) {
                        // 等待线程完成
                        WaitForSingleObject(thread, INFINITE);
                        DWORD exitCode;
                        GetExitCodeThread(thread, &exitCode);
                        result = (exitCode != 0); // 非零表示LoadLibrary成功
                        CloseHandle(thread);
                    }
                }
            }
            VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        }
        CloseHandle(process);
    }
    
    return result;
}

// 关闭Steam进程
bool KillSteamProcess() {
    DWORD steamProcessId = GetProcessIdByName(L"steam.exe");
    if (steamProcessId == 0) {
        return true; // Steam未运行
    }
    
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, steamProcessId);
    if (!process) {
        return false;
    }
    
    bool result = TerminateProcess(process, 0);
    CloseHandle(process);
    
    // 等待Steam完全关闭
    Sleep(2000);
    return result;
}

// 从文件加载AppIDs
bool LoadAppIdsFromFile(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    g_appIds.clear();
    unsigned int appId;
    while (file >> appId) {
        g_appIds.push_back(appId);
    }
    
    file.close();
    return !g_appIds.empty();
}

// 生成修改后的DLL
bool GenerateHookDLL(const wchar_t* dllPath) {
    // 实际实现中，这里会动态生成或修改DLL
    // 此处简化为返回已存在DLL的情况
    return true;
}

// 主函数
int main() {
    // 1. 加载用户选择的游戏AppID列表
    if (!LoadAppIdsFromFile("appids.txt")) {
        std::cout << "Failed to load AppIDs." << std::endl;
        return 1;
    }
    
    // 2. 生成或准备需要注入的DLL
    wchar_t dllPath[MAX_PATH] = L"GreenLumaHook.dll";
    if (!GenerateHookDLL(dllPath)) {
        std::cout << "Failed to prepare hook DLL." << std::endl;
        return 1;
    }
    
    // 3. 确保Steam已关闭
    if (!KillSteamProcess()) {
        std::cout << "Failed to terminate Steam. Make sure it's not running." << std::endl;
        return 1;
    }
    
    // 4. 启动Steam客户端
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi;
    
    // Steam安装路径
    wchar_t steamPath[MAX_PATH] = L"C:\\Program Files (x86)\\Steam\\steam.exe";
    
    if (!CreateProcessW(NULL, steamPath, NULL, NULL, FALSE, 
                        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cout << "Failed to start Steam." << std::endl;
        return 1;
    }
    
    // 5. 注入DLL到Steam进程
    if (!InjectDLL(pi.dwProcessId, dllPath)) {
        std::cout << "Failed to inject DLL." << std::endl;
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }
    
    // 6. 恢复Steam进程执行
    ResumeThread(pi.hThread);
    
    // 清理句柄
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    std::cout << "Steam started with the hook DLL successfully injected!" << std::endl;
    std::cout << "Emulating ownership of " << g_appIds.size() << " games." << std::endl;
    
    return 0;
}