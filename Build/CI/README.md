# PR 云端检查与本地打包

本项目使用 GitHub 托管的 Ubuntu Runner 检查 PR，在本地 Windows 电脑完成 UE 编译和打包。云端无需安装 UE、购买云服务器、配置自托管 Runner 或提供引擎访问凭据。当前交付是 PR 检查与本地构建脚本；没有部署游戏服务器或自动发布运行包。

## 1. 在 GitHub 启用

1. 将本目录、[工作流](../../.github/workflows/pr-checks.yml)、新增 Server Target 和相关配置提交并推送到 `master`。
2. 在仓库 **Settings → Actions → General** 中确认允许本工作流使用的官方 Actions。
3. 打开 **Actions → PR Checks → Run workflow**，选择 `master`，先手动执行一次。
4. 后续创建功能分支并向 `master` 提 PR，会自动出现 **Repository checks** 检查。
5. 如需阻止未通过检查的 PR 合并，在 **Settings → Rules → Rulesets** 或分支保护设置中，为 `master` 添加该必需状态检查。先成功运行一次，再从列表选择实际检查名称。

工作流使用只读仓库权限、`pull_request` 事件和 GitHub 托管 Runner，不需要 Secrets。无需为 PR 注册本地 Runner。仓库主分支若改名，需要同步修改工作流的目标分支。

## 2. 检查范围

| 检查 | 覆盖内容 |
| --- | --- |
| 项目文本 | UTF-8、未解决的合并冲突标记 |
| 配置 | 项目自有 JSON／uproject 和 YAML 的语法 |
| 文档 | 行内相对文件／图片链接、代码围栏配对 |
| 自动化脚本 | Python 检查器的回归用例、PowerShell 语法 |
| Lua | `Content/Script` 中项目脚本的 Lua 5.4 语法，不执行游戏逻辑 |

目前已引入 UnLua 插件，但尚未发现 `Content/Script` 下的项目 Lua 脚本，Lua 检查会明确显示跳过。接入业务脚本时固定 UnLua 的 Lua 版本；如使用其他 Lua 主次版本，必须同时修改工作流中的解释器安装项和检查器的版本校验。语法通过不证明 UE 绑定调用或脚本打包正确。

云端不编译 UE C++，不 Cook 资源，不验证材质、资产引用、联机行为及性能。YAML 检查只检查语法，不代替 GitHub 的工作流语义验证；文档检查不请求外部网址，也不验证页内标题锚点。完整项目验证仍在本地进行。

本地复现文本检查：

```powershell
python -m pip install -r Build/CI/requirements.txt
python -m unittest discover -s Build/CI -p 'test_*.py' -v
python Build/CI/check_repository.py
```

未来存在 Lua 文件时，本地还需将 Lua 5.4 的 `luac` 放入 PATH。

## 3. 本地打包

使用 PowerShell 7，在项目根目录执行。先关闭正在运行的项目编辑器，避免编译与 Live Coding 冲突。

```powershell
# 当前安装版 UE：只检查环境，不编译。
pwsh -NoProfile -File Build/CI/Build-Local.ps1 -EngineRoot 'D:\Unreal Engine\UE5\UE_5.8' -BuildKind Game -PreflightOnly

# 生成 Development 玩家运行包；默认输出到临时目录下的新建独立目录。
pwsh -NoProfile -File Build/CI/Build-Local.ps1 -EngineRoot 'D:\Unreal Engine\UE5\UE_5.8' -BuildKind Game

# 完成 UE 5.8.3 源码版引擎构建后，将路径替换为实际源码版引擎根目录。
pwsh -NoProfile -File Build/CI/Build-Local.ps1 -EngineRoot 'D:\UE_Source\UE5.8.3' -BuildKind All
```

`Game` 使用现有 Game Target 生成玩家客户端；`Server` 使用新增的 Server Target；`All` 顺序生成两者。`-Configuration Shipping` 可切换发布配置。可用 `-OutputDirectory` 指定一个新的或空的目录，脚本不会删除旧产物。默认只 Cook 第一人称模板地图；在 PowerShell 中调用脚本并传入 `-Maps @('/Game/FirstPerson/Lvl_FirstPerson', '/Game/OtherMap')` 可选择多个地图。

本机 UE 5.8.3 是安装版，目前没有登记 Win64 Server 构建支持。脚本对 Server／All 要求源码版引擎，提前拒绝安装版；新增 Target 本身不能补齐引擎的 Server 构建能力。Server 默认地图已指向现有第一人称地图，后续应随正式比赛地图更新。

每次输出包含运行包、UAT 日志和 `build-info.json`（提交号、工作区是否有未提交改动、引擎版本、构建配置、地图及时间）。打包成功之后还要做独立服务器与客户端测试；没有将进程启动或打包成功视作联网玩法通过。

## 4. 后续接入点

按[开发路线图](../../Docs/Design/00-overview.md#6-客户端与服务端开发路线图)推进：M1～M2 增加服务器与两个客户端的战斗、单局验证及共享规则测试；M3 增加大厅匹配往返与失败路径验收；M4～M5 在有 GPU 的本地环境完成 Shader、破坏性能及多机连续对局验收。大厅／匹配服务的构建与启动方式待选型后补充，当前脚本不负责部署它。发布运行包时，客户端与战斗服务器使用同一份已验证构建的产物，并同步提供匹配服务所需配置。

参考：[GitHub PR 事件](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request)、[必需状态检查](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)、[UE Dedicated Server](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)。
