# 基础战斗 HUD 接口

## 1. 模块职责与状态

本模块实现 F09-01 当前阶段范围内的准星、生命值与血条、弹匣／备用弹药、换弹状态与进度显示，并提供 F09-03 的命中、受伤、死亡倒计时及重生保护显示层。C++ 创建原生 UMG 布局并提供只显示数据的更新接口，Lua 负责正式数据与开发预览之间的显示流程。

- 调用方：模板路径使用本地 `AShooterPlayerController`；正式 Battle 路径使用 `ABattlePlayerController`（见第 8 节），由武器复制状态和后续生命状态接收模块调用公开更新函数。
- 执行端：拥有本地玩家的客户端；Dedicated Server 不创建控件。
- 权威归属：本模块没有玩法权威，不扣血、不消耗弹药、不决定换弹开始或完成，也不发送 RPC。
- 当前实现：F09-03 原生控件、Lua 显示流程和本地预览已加入；正式 Battle 路径已接入 F05 拥有者可见的弹匣、备用弹药、换弹状态和服务器开始时间。命中确认、权威生命、死亡截止时间和重生保护数据仍待 F06，因此 F09-03 联机验收未完成。
- 设计依据：[客户端设计](../Design/02-client.md)、[玩法设计](../Design/01-gameplay.md)。

相关源码：

- `Source/Project_FPS/Variant_Shooter/UI/BattleHUDWidget.h/.cpp`
- `Source/Project_FPS/Variant_Shooter/ShooterPlayerController.h/.cpp`
- `Source/Project_FPS/Battle/BattlePlayerController.h/.cpp`
- `Source/Project_FPS/Battle/Components/BattleWeaponComponent.h/.cpp`
- `Content/Script/Client/UI/BattleHUD.lua`

## 2. 正式显示接口

以下函数均由 `AShooterPlayerController` 和 `ABattlePlayerController` 提供为本地 `BlueprintCallable` 接口。输入会转交给 Lua，再由 Lua 调用原生控件的渲染函数。接口无返回值，不改变角色、武器或比赛状态。

### `UpdateBattleHUDHealth(CurrentHealth, MaxHealth)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `CurrentHealth` | `float` | 当前生命值；显示层限制到 `0..MaxHealth` |
| `MaxHealth` | `float` | 最大生命值；小于 0 时按 0 显示 |

当前 Shooter 模板只提供 `OnDamaged(float LifePercent)`。临时适配按设计基线 100 将比例换算为当前生命值；后续复制状态可直接调用本接口提供真实当前值和最大值。

> **已知冲突（待与 F06 负责人对齐）**：当前模板角色 `AShooterCharacter::MaxHP` 为 `500`（`Source/Project_FPS/Variant_Shooter/ShooterCharacter.h`），与[玩法设计](../Design/01-gameplay.md)的生命值 `100` 冲突。本模块的 `OnPawnDamaged` 适配按 100 显示，不代表模板角色的真实血量比例。生命基线由 F06（生命、伤害、死亡与重生）统一处理；在 F06 接入前，该显示**不得**视为权威生命值。S1（F08-01）未改动任何生命字段。

### `UpdateBattleHUDAmmo(MagazineAmmo, ReserveAmmo)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `MagazineAmmo` | `int32` | 当前弹匣内子弹数；显示层最小为 0 |
| `ReserveAmmo` | `int32` | 当前备用弹药数；显示层最小为 0 |

Shooter 模板的 `OnBulletCountUpdated` 只提供弹匣容量和当前弹数，没有备用弹药，因此模板适配仍把备用弹药显示为 0。正式 Battle 路径订阅 `UBattleWeaponComponent::OnWeaponStateChanged`，同时读取拥有者可见的 `MagazineAmmo` 与 `ReserveAmmo`，绑定后也会主动读取一次完整快照，避免错过早于 Controller 绑定发生的 RepNotify。

### `UpdateBattleHUDReload(bIsReloading, NormalizedProgress)`

| 参数 | 类型 | 约束与含义 |
| --- | --- | --- |
| `bIsReloading` | `bool` | 是否显示换弹提示；`false` 时隐藏提示与进度条 |
| `NormalizedProgress` | `float` | `0..1` 的换弹进度，越界值由显示层限制 |

正式 Battle 路径读取复制的 `bIsReloading` 与 `ReloadStartServerTime`，使用 `AGameStateBase::GetServerWorldTimeSeconds()` 和武器配置的 `ReloadDuration` 计算归一化进度。仅换弹期间以 0.05 秒本地计时器刷新显示；完成、中断、Pawn 切换或 Controller 结束时清除计时器并传入 `false, 0`。该计时器只计算表现进度，不决定换弹完成或弹药转移。

最小调用示例：

```cpp
PlayerController->UpdateBattleHUDHealth(CurrentHealth, MaxHealth);
PlayerController->UpdateBattleHUDAmmo(CurrentMagazineAmmo, ReserveAmmo);
PlayerController->UpdateBattleHUDReload(bReloading, ReloadProgress);
```

## 3. Lua 与原生控件契约

### F09-03 显示入口

以下同样是本地 `AShooterPlayerController` 的 `BlueprintCallable`、无返回值入口，不发送 RPC、不改变生命或重生状态。仅拥有者客户端创建 HUD；Dedicated Server 调用不会创建控件。

| 入口 | 输入及含义 | 调用约束 |
| --- | --- | --- |
| `UpdateBattleHUDHitConfirmed()` | 无 | 仅在 F05 的服务器确认命中事件经过调用方去重后触发；重复调用会重启本地 0.35 秒标记 |
| `UpdateBattleHUDDamageFeedback()` | 无 | 仅在 F06 的权威受伤事件经过调用方去重后触发；重复调用会重启本地 0.6 秒红色提示 |
| `UpdateBattleHUDDeath(bIsDead, RemainingSeconds)` | `bool` 死亡状态；`float` 距服务器确认重生时刻的剩余秒数，非负、有限 | 调用方从服务器时间和截止时刻计算并持续刷新；`false` 隐藏提示；显示层不自行决定重生 |
| `UpdateBattleHUDProtection(bProtected, RemainingSeconds)` | `bool` 保护状态；`float` 距服务器确认保护结束时刻的剩余秒数，非负、有限 | 调用方从服务器时间和截止时刻计算并持续刷新；`false` 隐藏提示；显示层不自行决定保护取消 |

新入口目前没有正式 F05/F06 调用方。当前模板 `OnDamaged(float LifePercent)` 仍只驱动生命条，不触发服务器确认的命中、受伤或保护提示。时间值只有当前输入的显示意义；正式状态应在服务器通知、倒计时刷新及结束时继续传入。调用方负责按 `RoundId`、`LifeId`、Pawn 和事件编号过滤旧局、旧生命期与重复通知；本接口暂不携带这些标识。

最小接入示例（待 F05/F06 实现，不代表当前已接线）：

```cpp
// 在权威事件已确认且标识检查通过后，由拥有者客户端调用。
PlayerController->UpdateBattleHUDHitConfirmed();
PlayerController->UpdateBattleHUDDeath(true, ServerRespawnRemainingSeconds);
PlayerController->UpdateBattleHUDProtection(false, 0.0f);
```

`UBattleHUDWidget` 实现 `IUnLuaInterface`，模块名固定为 `Client.UI.BattleHUD`，对应 `Content/Script/Client/UI/BattleHUD.lua`。

Lua 接收 `BP_ReceiveHealthState`、`BP_ReceiveAmmoState` 和 `BP_ReceiveReloadState`，缓存正式数据，并调用以下 C++ 渲染端点：

- `SetHealthDisplay(float CurrentHealth, float MaxHealth)`
- `SetAmmoDisplay(int32 MagazineAmmo, int32 ReserveAmmo)`
- `SetReloadDisplay(bool bIsReloading, float NormalizedProgress)`
- `SetHitDisplay(bool bVisible)`、`SetDamageDisplay(float Opacity)`
- `SetDeathDisplay(bool bIsDead, float RemainingSeconds)`、`SetProtectionDisplay(bool bProtected, float RemainingSeconds)`

F09-03 的 Lua 接收事件为 `BP_ReceiveHitConfirmed`、`BP_ReceiveDamageFeedback`、`BP_ReceiveDeathState`、`BP_ReceiveProtectionState` 和 `BP_ResetCombatFeedback`。命中与受伤是本地短暂表现；死亡与保护状态只由外部刷新，Lua 不推断权威状态变化。受伤透明度在显示端限制到 `0..0.35`，倒计时显示不小于 0 秒。

上述渲染端点只更新文本、颜色、可见性和进度条。准星由 C++ 原生布局固定在视口中心；生命面板锚定左下角，弹药面板锚定右下角。
重生保护面板锚定顶部中央，距视口顶部 104 个 UMG 布局单位，避开 Shooter 模板顶部的比分显示。

## 4. 开发预览

预览默认关闭，配置项为：

```ini
[/Script/Project_FPS.ShooterPlayerController]
bEnableBattleHUDPreview=False
```

PIE 或本地游戏运行时可在控制台使用。每次重新启动 PIE 后预览默认关闭，必须先输入 `HUDPreview 1`，再输入 `HUDPreviewState <编号>`；单独输入状态命令不会开启预览，也不会在控制台打印结果。

| 命令 | 效果 |
| --- | --- |
| `HUDPreview 1` | 启用本地预览 |
| `HUDPreview 0` | 关闭预览并恢复最近一次正式数据 |
| `HUDPreviewState 0` | 自动循环下列 1～9 状态 |
| `HUDPreviewState 1` | 固定显示满血与 `30 / 90` 弹药 |
| `HUDPreviewState 2` | 固定显示受伤与弹药变化 |
| `HUDPreviewState 3` | 固定显示低弹匣状态 |
| `HUDPreviewState 4` | 固定显示循环换弹进度 |
| `HUDPreviewState 5` | 固定显示受伤红色提示 |
| `HUDPreviewState 6` | 固定显示命中标记 |
| `HUDPreviewState 7` | 模拟 3 秒死亡倒计时 |
| `HUDPreviewState 8` | 模拟 1.5 秒重生保护提示 |
| `HUDPreviewState 9` | 模拟重生后的正常 HUD |

预览数据只存在于本地控件的 Lua 实例中，不调用正式更新源、不修改 Pawn／Weapon，不发送网络请求或 RPC。多个 PIE 本地玩家各自持有独立控件与 Lua 实例。

`HUDPreview 1` 只覆盖画面；期间受到的真实伤害仍由模板角色计算和缓存。输入 `HUDPreview 0` 后显示实际生命值，例如此前受伤至 70 时会显示 `70 / 100`，不是预览扣血。预览中的 3 秒死亡倒计时也不会触发角色重生。

## 5. 生命周期、时序与错误处理

- `EnsureBattleHUD` 仅允许本地 PlayerController 且非 Dedicated Server 创建一个 HUD，重复调用不会重复创建。
- `OnPossess` 使用唯一动态绑定订阅模板角色事件；`OnUnPossess`、Pawn 销毁与 Controller `EndPlay` 都移除绑定。
- 正式 `ABattlePlayerController` 在 `BeginPlay`、`OnPossess` 和拥有者客户端 `OnRep_Pawn` 统一刷新 Pawn 绑定；绑定武器组件后立即读取权威快照，组件 RepNotify 再驱动后续刷新。
- 正式 Battle Pawn 解除占有、销毁或 Controller `EndPlay` 时移除武器状态委托并清除换弹 HUD 计时器；重复刷新同一 Pawn 不重复绑定。
- Shooter 模板的死亡计时器到期后，角色先保存 Controller 和角色类，再销毁旧 Pawn 并显式调用 Controller 的 `RespawnTemplateCharacter`。UE 5.8 在 Pawn 的 `OnDestroyed` 广播前已解除占有，因此不能依赖该广播触发重生。Controller 优先使用本队标记的 PlayerStart，若没有则使用地图中的任意 PlayerStart；角色类未配置时回退到旧 Pawn 的类。此兼容路径仍是模板本地重生，不是 F06 的权威重生调度。
- `EndPlay` 从视口移除 HUD 和触控控件；Widget 销毁时停止 Lua 显示流程。
- 角色销毁时清空弹药显示并隐藏换弹提示；新 Pawn 重新绑定模板事件。
- Pawn 切换、销毁和 Controller 结束时重置 F09-03 的短暂表现、死亡和保护状态；新 Pawn 接管后由未来的权威状态接收方重新填充。预览开启期间重置正式状态，不改变当前预览帧；关闭预览后恢复最新正式状态。
- 命中与受伤在预览开启时不进入正式表现缓存；预览关闭时清除预览的短暂效果。死亡与保护的正式数据在预览期间仍缓存，关闭预览时恢复。
- 无效的控件类或创建失败只记录本地错误日志，不影响服务器权威状态。
- 当前接口不包含 `MatchId`、`RoundId` 或 `LifeId`。调用方在后续接入异步／复制状态时必须先过滤旧生命期、旧回合和旧 Pawn 的迟到数据，本显示接口不会自行判断数据新旧。

## 6. 兼容性与验证

- 旧 `BulletCounterUIClass` 属性保留为弃用兼容字段，Controller 不再创建旧 `UI_ShooterBulletCounter`，避免与新 HUD 重叠。
- 项目显式启用仓库内现有 UnLua 2.3.6，并在 Runtime 模块中增加 UnLua、SlateCore 依赖；未升级引擎或插件。
- 仓库文本、JSON、Lua 语法和 C++ 编译结果以本次变更的交付记录为准。
- 本次新增的 F09-03 控件实际渲染、不同窗口尺寸、多 PIE 与独立服务器行为在完成相应运行验证前均标记为待验证。

## 7. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-08 | 新增 F09-01 基础 HUD、Lua 显示流程、本地预览与模板事件适配 | 新增三个正式显示入口和两个本地预览命令；无网络协议 | 仓库检查与可用编译环境结果见交付说明；UE 运行渲染待人工验证 |
| 2026-11-09 | 修正 Controller 内部解绑辅助函数的参数命名，适配 UE 严格编译检查 | 对外契约不变 | UE 5.8.4 `Project_FPSEditor Win64 Development` 编译通过；PIE 已观察到准星、生命和当前弹匣显示，模拟换弹待验证 |
| 2026-11-12 | S1 在 `ABattlePlayerController` 迁移同一组 HUD 接口与本地预览；补充模板 `MaxHP=500` 与设计 100 的冲突说明 | 新增 `ABattlePlayerController` 上的同名接口；模板控制器接口不变 | C++ 编译与仓库文本检查待执行；Battle 控制器上的 HUD 渲染待验证 |

## 8. `ABattlePlayerController` 上的迁移接口

S1（F08-01）为正式对战新建 `ABattlePlayerController`（继承 `AProject_FPSPlayerController`）。当前已迁移 `EnsureBattleHUD`、全部 F09-01／F09-03 显示入口、`HUDPreview`、`HUDPreviewState` 与输入映射上下文处理。契约与第 2、4 节一致：

- 执行端：拥有本地玩家的客户端；Dedicated Server 不创建控件。
- 权威归属：与模板控制器相同，不改变角色、武器或比赛状态，不发送 RPC。
- 复用同一控件类 `UBattleHUDWidget` 与 `Content/Script/Client/UI/BattleHUD.lua`。
- **未迁移** `AShooterPlayerController::OnPawnDestroyed` 的模板重生逻辑；重生由 F06-04 统一调度。`BeginPlay` 也不再写入模板的比例式生命伪数据。
- F05 武器状态已经接入：`ABattlePlayerController` 订阅当前 `ABattleCharacter` 的 `UBattleWeaponComponent`，驱动弹药和换弹 HUD。
- F06 生命、受伤、死亡、重生和保护状态尚未接入；四个 F09-03 显示入口已提供，但必须由未来的权威接收方在 RoundId／LifeId 过滤后调用。

| 2026-11-14 | 增加 F09-03 显示入口、Lua 短暂反馈、本地预览和 Pawn 生命周期清理 | 新增四个本地显示入口，无网络协议；正式事件调用方待 F05/F06 | UE 5.8.4 Editor Development 编译及 Lua 5.4.3 语法／流程检查通过；仓库检查仅因缺少 `luac5.4` 未通过；新增画面与双客户端权威联调待验证 |
| 2026-11-15 | 修复模板角色死亡后未重生；说明关闭预览后恢复实际生命值 | F09-03 显示入口不变；模板重生改为死亡计时器显式调用 Controller，补充无队伍出生点的回退 | UE 5.8.4 Editor Development 编译通过；用户确认实际死亡后重新生成并恢复控制，日志出现 `Template respawn possessed`；新增反馈的完整预览仍待验证 |
| 2026-11-15 | 下移重生保护面板，避开顶部比分 | 对外契约不变；仅布局纵向位置由 32 调整为 104 | UE 5.8.4 Editor Development 编译通过；用户确认 PIE 中布局调整可用 |
| 2026-11-17 | 正式 Battle 控制器补齐 F09-03 入口并接入 F05 权威弹药、备用弹药与换弹进度 | 武器组件增加统一状态变化通知；Battle Controller 增加 Pawn／组件绑定、解绑和本地进度计时器；无新 RPC | UE 5.8 `Project_FPSEditor Win64 Development` 编译通过；双端 PIE 待验证；仓库检查仅因缺少 `luac5.4` 未通过 |
