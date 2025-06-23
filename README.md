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
