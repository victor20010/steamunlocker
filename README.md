# steamunlocker
SteamUnlocker - Steam游戏解锁工具
SteamUnlocker是一个基于C++开发的工具，通过动态生成DLL并注入Steam进程，实现模拟拥有特定游戏的功能。该工具使用API钩取技术修改Steam的授权检查机制，让Steam认为用户拥有指定的游戏。
功能特点
动态DLL生成：运行时根据用户输入的游戏AppID生成定制化DLL

进程注入：将生成的DLL注入到Steam进程实现功能修改

AppID管理：支持添加多个游戏AppID同时解锁

自动化流程：自动关闭Steam、注入DLL并重启Steam

临时文件清理：使用后自动清理生成的临时文件
技术实现：
![deepseek_mermaid_20250623_6ca322](https://github.com/user-attachments/assets/910a3e4f-ce5f-4f00-83a9-ee010eff9ac8)


使用说明
编译项目
使用Visual Studio打开项目

配置为Release x86模式

编译生成可执行文件

运行程序

以管理员身份运行steamunlocker.exe

输入想要解锁的游戏AppID（每行一个）

输入0结束输入

程序将自动处理后续流程

命令行使用（可选）
steamunlocker.exe 730 570 440 # 解锁CS:GO, Dota 2, TF2


免责声明

⚠️ 此工具仅用于教育目的

⚠️ 不得用于盗版或非法用途

⚠️ 使用可能导致Steam账号封禁

⚠️ 支持开发者，请购买正版游戏


技术细节：


核心组件：


1.DLL模板引擎：


const char* DLL_TEMPLATE = R"(
 
  // DLL源代码模板
  
  __declspec(dllexport) int g_appIdCount = %d;
 
  __declspec(dllexport) int g_appIds[] = {%s};
 
  // API钩取实现...

)";


2.进程管理：

Steam进程查找与终止

进程注入技术

权限管理

3.AppID处理：

用户输入处理

动态数组管理

数据序列化
注入原理：
sequenceDiagram
    participant UserApp
    participant SteamClient
    participant CustomDLL
    
    UserApp->>CustomDLL: 1. 生成定制DLL
    UserApp->>SteamClient: 2. 终止Steam进程
    UserApp->>SteamClient: 3. 启动并注入DLL
    SteamClient->>CustomDLL: 4. 加载DLL
    SteamClient->>CustomDLL: 5. 调用IsSubscribedApp
    CustomDLL-->>SteamClient: 6. 返回修改后的授权结果

![deepseek_mermaid_20250623_6ad26a](https://github.com/user-attachments/assets/5f1dff8b-0540-4d8c-9408-5fdc7f789c1a)

关于GL原版内核：

核心原理：

GreenLuma 通过修改 Steam 客户端的 AppID 验证机制，欺骗 Steam 客户端认为用户拥有特定游戏

它使用 DLL 注入技术，将自定义代码注入到 Steam 进程中

技术实现细节：

游戏识别：通过 Steam 的 AppID 系统识别游戏

进程挂钩：截获并修改 Steam 验证游戏所有权的 API 调用

内存操作：在运行时修改 Steam 客户端的内存数据

GreenLumaGUI 的特定实现：

使用 WPF (Windows Presentation Foundation) 构建用户界面

提供游戏搜索功能：连接到 Steam 数据库 API 获取游戏信息

创建和管理 AppID 列表：保存用户想要"模拟拥有"的游戏

自动处理 Steam 客户端的启动和注入过程

工作流程：

用户通过界面搜索并选择游戏

程序记录这些游戏的 AppID

启动时，程序先关闭已运行的 Steam

然后启动修改后的 Steam 客户端，注入必要的代码

修改后的 Steam 会将选定的游戏显示为"已拥有"状态

GreenLumaGUI 本质上是在原有 GreenLuma 的功能基础上提供了一个更友好的中文图形界面，简化了配置和使用过程，使非技术用户也能轻松使用这些功能。

GreenLuma 进行的内存修改操作具体包括以下技术细节：

DLL 注入机制:

首先使用 Windows API 函数如 CreateRemoteThread 和 LoadLibrary 将自定义 DLL 注入到 Steam 进程的地址空间

这允许自定义代码在 Steam 进程的上下文中执行


API 函数挂钩 (Hooking):

通过修改 Steam 客户端中关键函数的内存地址指向自定义实现主要针对 Steam 验证游戏所有权的函数，例如检查用户是否拥有特定 AppID 的函数

使用了类似 Detours 或 MinHook 等挂钩库的技术


内存数据结构修改:


定位 Steam 客户端内存中存储游戏库和许可证信息的数据结构

直接修改这些数据结构中的值，如添加特定游戏的 AppID 到已授权游戏列表

可能涉及修改指针、数组或哈希表等数据结构


运行时补丁:

在 Steam 客户端调用验证函数时，截获这些调用并返回"成功"结果

对于特定的 AppID 查询，修改返回值使其表明用户拥有该游戏


签名验证绕过:

可能还包括绕过 Steam 的内部完整性检查机制

修改或禁用 Steam 用来验证其内存没有被篡改的代码



这种内存修改技术在技术上类似于游戏修改器（Game Trainer）和某些反作弊系统，但用途不同。GreenLumaGUI 本身并不直接实现这些低级内存操作，而是提供图形界面来配置和启动原始 GreenLuma 工具，后者才是实际执行这些内存修改的组件。

API 挂钩的具体实现方式

函数地址定位：

首先识别 Steam 客户端中负责验证游戏所有权的关键函数

通常通过分析 Steam 客户端的二进制文件找到这些函数的内存地址

目标函数可能包括 ISteamApps::BIsSubscribedApp() 或类似函数


内存写入权限获取：

使用 Windows API 函数 VirtualProtectEx() 修改目标函数所在内存页的权限

将内存页标记为可读可写可执行（PAGE_EXECUTE_READWRITE）


函数序言（Prologue）替换：

修改原始函数的开头几个字节，插入跳转指令（通常是 JMP 指令）

跳转指令将执行流程重定向到自定义的替代函数

典型的替换方式是使用相对或绝对跳转指令


替代函数实现：

创建一个自定义函数，用于替代原始的所有权验证函数

对于特定的 AppID 参数（用户选择的游戏），始终返回 "true"（已拥有）

对于其他调用，可能会调用原始函数以保持其他功能正常


跳板（Trampoline）机制：

保存原始函数开头被覆盖的指令

创建"跳板"区域，包含这些保存的指令和跳回原始函数剩余部分的指令

这允许替代函数在需要时调用原始函数逻辑


注意事项
需要以管理员权限运行

杀毒软件可能误报（添加信任或临时禁用）

Steam更新后可能需要调整偏移地址

每次重启Steam后需要重新运行工具

贡献指南
欢迎提交Pull Request改进项目：

Fork本项目

创建特性分支

提交更改

推送分支并创建Pull Request

许可证
本项目采用GLP3.0许可证
