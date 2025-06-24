// HookDLL.cpp
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <shlobj.h>
#include "MinHook.h" // 包含MinHook头文件

// 在VS中，这会自动链接正确的库（需在项目设置中指定库目录）
#pragma comment(lib, "minhook.x86.lib") 

// 全局变量
const char* TEMP_APPID_FILE = "steam_appids.tmp";
std::vector<int> g_ownedAppIds;

// 原始函数指针的类型定义和变量
using IsSubscribedAppFn = bool(__thiscall*)(void* thisPtr, unsigned int appId);
IsSubscribedAppFn oIsSubscribedApp = nullptr;

// 我们的钩子函数，用于替换原始函数
// __fastcall是MinHook推荐的调用约定，this指针在ecx中传递
bool __fastcall HookedIsSubscribedApp(void* thisPtr, void* edx_unused, unsigned int appId) {
    // 检查请求的AppID是否在我们伪造的列表中
    for (int ownedId : g_ownedAppIds) {
        if (ownedId == appId) {
            // 如果是，直接返回true，告诉Steam我们拥有它
            return true;
        }
    }
    // 如果不在我们的列表中，调用原始的IsSubscribedApp函数，保持正常功能
    return oIsSubscribedApp(thisPtr, appId);
}

// 特征码扫描函数，用于在内存中定位目标函数，比硬编码偏移量更可靠
void* FindPattern(const char* moduleName, const char* pattern, const char* mask) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) return nullptr;

    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(MODULEINFO));

    char* startAddr = (char*)moduleInfo.lpBaseOfDll;
    char* endAddr = startAddr + moduleInfo.SizeOfImage;
    size_t patternLen = strlen(mask);

    for (char* p = startAddr; p < endAddr - patternLen; p++) {
        bool found = true;
        for (size_t i = 0; i < patternLen; i++) {
            if (mask[i] != '?' && pattern[i] != *(p + i)) {
                found = false;
                break;
            }
        }
        if (found) {
            return p;
        }
    }
    return nullptr;
}

// 从临时文件中加载AppID列表
void LoadAppIDs() {
    char tempPath[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, tempPath))) {
        return;
    }

    std::string fullPath = std::string(tempPath) + "\\" + TEMP_APPID_FILE;
    std::ifstream inFile(fullPath);
    if (!inFile.is_open()) return;

    int id;
    while (inFile >> id) {
        g_ownedAppIds.push_back(id);
    }
    inFile.close();
    // 读取后立即删除临时文件，保持系统干净
    DeleteFileA(fullPath.c_str());
}

// 初始化钩子的主线程函数
DWORD WINAPI InitializeHook(LPVOID lpParam) {
    // 等待steamclient.dll被加载到进程中，因为注入时它可能还不存在
    while (GetModuleHandleA("steamclient.dll") == NULL) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 初始化MinHook库
    if (MH_Initialize() != MH_OK) {
        return 1;
    }
    
    // 使用特征码查找IsSubscribedApp函数
    // 注意: 此特征码可能随Steam更新而失效，需要时可查找新的
    // "55 8B EC 83 EC 08 8B 45 08 85 C0" -> "\x55\x8B\xEC\x83\xEC\x08\x8B\x45\x08\x85\xC0"
    const char* pattern = "\x55\x8B\xEC\x83\xEC\x08\x8B\x45\x08";
    const char* mask = "xxxxxxxxx";
    void* targetFunction = FindPattern("steamclient.dll", pattern, mask);
    
    if (!targetFunction) {
        // 如果找不到，无法挂钩，清理并退出
        MH_Uninitialize();
        return 1;
    }

    // 创建钩子：将目标函数重定向到我们的HookedIsSubscribedApp
    // oIsSubscribedApp将被填充为指向原始函数代码的跳板（trampoline）
    if (MH_CreateHook(targetFunction, &HookedIsSubscribedApp, reinterpret_cast<void**>(&oIsSubscribedApp)) != MH_OK) {
        MH_Uninitialize();
        return 1;
    }

    // 启用我们刚刚创建的钩子
    if (MH_EnableHook(targetFunction) != MH_OK) {
        MH_Uninitialize();
        return 1;
    }

    return 0;
}

// DLL入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            // 优化：禁止此DLL的线程库调用通知，避免死锁
            DisableThreadLibraryCalls(hModule);
            // 从文件加载要模拟的AppID
            LoadAppIDs();
            // 创建一个新线程来初始化钩子，避免阻塞DllMain
            CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
            break;
            
        case DLL_PROCESS_DETACH:
            // 当DLL被卸载时（例如Steam关闭），清理并移除所有钩子
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            break;
    }
    return TRUE;
}