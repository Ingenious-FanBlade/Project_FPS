# 基础战斗 HUD 接口

## 1. 模块职责与状态

本模块实现 F09-01 当前阶段范围内的准星、生命值与血条、弹匣／备用弹药、换弹状态与进度显示。C++ 创建原生 UMG 布局并提供只显示数据的更新接口，Lua 负责正式数据与开发预览之间的显示流程。

- 调用方：本地 `AShooterPlayerController`，后续可由生命、武器和复制状态接收模块调用公开更新函数。
- 执行端：拥有本地玩家的客户端；Dedicated Server 不创建控件。
- 权威归属：本模块没有玩法权威，不扣血、不消耗弹药、不决定换弹开始或完成，也不发送 RPC。
- 当前实现：原生控件、Lua 绑定、生命周期清理和本地预览已实现；UE 5.8.4 PIE 已观察到准星、生命和当前弹匣显示，模拟换弹流程及独立 Client／Server 包仍待验证。
- 设计依据：[客户端设计](../Design/02-client.md)、[玩法设计](../Design/01-gameplay.md)。

相关源码：

- `Source/Project_FPS/Variant_Shooter/UI/BattleHUDWidget.h/.cpp`
- `Source/Project_FPS/Variant_Shooter/ShooterPlayerController.h/.cpp`
- `Content/Script/Client/UI/BattleHUD.lua`

## 2. 正式显示接口

以下函数均为 `AShooterPlayerController` 的本地 `BlueprintCallable` 接口。输入会转交给 Lua，再由 Lua 调用原生控件的渲染函数。接口无返回值，不改变角色、武器或比赛状态。

### `UpdateBattleHUDHealth(CurrentHealth, MaxHealth)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `CurrentHealth` | `float` | 当前生命值；显示层限制到 `0..MaxHealth` |
| `MaxHealth` | `float` | 最大生命值；小于 0 时按 0 显示 |

当前 Shooter 模板只提供 `OnDamaged(float LifePercent)`。临时适配按设计基线 100 将比例换算为当前生命值；后续复制状态可直接调用本接口提供真实当前值和最大值。

### `UpdateBattleHUDAmmo(MagazineAmmo, ReserveAmmo)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `MagazineAmmo` | `int32` | 当前弹匣内子弹数；显示层最小为 0 |
| `ReserveAmmo` | `int32` | 当前备用弹药数；显示层最小为 0 |

当前 Shooter 模板的 `OnBulletCountUpdated` 只提供弹匣容量和当前弹数，没有备用弹药。模板事件只临时更新当前弹数并把备用弹药显示为 0；这不是权威备用弹药实现。武器复制状态完成后必须同时传入拥有者可见的当前弹匣与备用弹药。

### `UpdateBattleHUDReload(bIsReloading, NormalizedProgress)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `bIsReloading` | `bool` | 是否显示换弹提示；`false` 时隐藏提示与进度条 |
| `NormalizedProgress` | `float` | `0..1` 的换弹进度，越界值由显示层限制 |

正式流程应根据服务器确认的换弹起止时间在本地计算归一化进度，并在死亡、切图、换弹取消或完成时传入 `false, 0`。当前仓库没有可复用的换弹状态或服务器时间接口，因此尚未接入权威数据。

最小调用示例：

```cpp
PlayerController->UpdateBattleHUDHealth(CurrentHealth, MaxHealth);
PlayerController->UpdateBattleHUDAmmo(CurrentMagazineAmmo, ReserveAmmo);
PlayerController->UpdateBattleHUDReload(bReloading, ReloadProgress);
```

## 3. Lua 与原生控件契约

`UBattleHUDWidget` 实现 `IUnLuaInterface`，模块名固定为 `Client.UI.BattleHUD`，对应 `Content/Script/Client/UI/BattleHUD.lua`。

Lua 接收 `BP_ReceiveHealthState`、`BP_ReceiveAmmoState` 和 `BP_ReceiveReloadState`，缓存正式数据，并调用以下 C++ 渲染端点：

- `SetHealthDisplay(float CurrentHealth, float MaxHealth)`
- `SetAmmoDisplay(int32 MagazineAmmo, int32 ReserveAmmo)`
- `SetReloadDisplay(bool bIsReloading, float NormalizedProgress)`

上述渲染端点只更新文本、颜色、可见性和进度条。准星由 C++ 原生布局固定在视口中心；生命面板锚定左下角，弹药面板锚定右下角。

## 4. 开发预览

预览默认关闭，配置项为：

```ini
[/Script/Project_FPS.ShooterPlayerController]
bEnableBattleHUDPreview=False
```

PIE 或本地游戏运行时可在控制台使用：

| 命令 | 效果 |
| --- | --- |
| `HUDPreview 1` | 启用本地预览 |
| `HUDPreview 0` | 关闭预览并恢复最近一次正式数据 |
| `HUDPreviewState 0` | 自动循环满血、受伤、低弹药和换弹 |
| `HUDPreviewState 1` | 固定显示满血与 `30 / 90` 弹药 |
| `HUDPreviewState 2` | 固定显示受伤与弹药变化 |
| `HUDPreviewState 3` | 固定显示低弹匣状态 |
| `HUDPreviewState 4` | 固定显示循环换弹进度 |

预览数据只存在于本地控件的 Lua 实例中，不调用正式更新源、不修改 Pawn／Weapon，不发送网络请求或 RPC。多个 PIE 本地玩家各自持有独立控件与 Lua 实例。

## 5. 生命周期、时序与错误处理

- `EnsureBattleHUD` 仅允许本地 PlayerController 且非 Dedicated Server 创建一个 HUD，重复调用不会重复创建。
- `OnPossess` 使用唯一动态绑定订阅模板角色事件；`OnUnPossess`、Pawn 销毁与 Controller `EndPlay` 都移除绑定。
- `EndPlay` 从视口移除 HUD 和触控控件；Widget 销毁时停止 Lua 显示流程。
- 角色销毁时清空弹药显示并隐藏换弹提示；新 Pawn 重新绑定模板事件。
- 无效的控件类或创建失败只记录本地错误日志，不影响服务器权威状态。
- 当前接口不包含 `MatchId`、`RoundId` 或 `LifeId`。调用方在后续接入异步／复制状态时必须先过滤旧生命期、旧回合和旧 Pawn 的迟到数据，本显示接口不会自行判断数据新旧。

## 6. 兼容性与验证

- 旧 `BulletCounterUIClass` 属性保留为弃用兼容字段，Controller 不再创建旧 `UI_ShooterBulletCounter`，避免与新 HUD 重叠。
- 项目显式启用仓库内现有 UnLua 2.3.6，并在 Runtime 模块中增加 UnLua、SlateCore 依赖；未升级引擎或插件。
- 仓库文本、JSON、Lua 语法和 C++ 编译结果以本次变更的交付记录为准。
- UE 中的控件实际渲染、不同窗口尺寸、多 PIE 与独立服务器行为在完成相应运行验证前均标记为待验证。

## 7. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-08 | 新增 F09-01 基础 HUD、Lua 显示流程、本地预览与模板事件适配 | 新增三个正式显示入口和两个本地预览命令；无网络协议 | 仓库检查与可用编译环境结果见交付说明；UE 运行渲染待人工验证 |
| 2026-11-09 | 修正 Controller 内部解绑辅助函数的参数命名，适配 UE 严格编译检查 | 对外契约不变 | UE 5.8.4 `Project_FPSEditor Win64 Development` 编译通过；PIE 已观察到准星、生命和当前弹匣显示，模拟换弹待验证 |
