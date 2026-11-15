# 武器系统接口（F05-01 / F05-03）

## 1. 模块职责与状态

本模块实现 F05-01 在 S3 第一阶段的范围：步枪配置数据资产、角色复制开启、出生装备与权威初始弹药。

- 实现状态：**F05-01 已实现**（数据资产定义、角色 `bReplicates`、服务器出生装备、权威弹药初始化）；**F05-03 已实现**（`Server_Fire` 请求入口、校验链、权威扣弹、表现通知占位）。
- **未实现**：本地开火与反馈（F05-02）、服务器射线／命中／伤害提交（F05-04）、换弹与中断（F05-05）、LifeId 校验（F06）。
- 调用方：服务器 `ABattleCharacter` 自身在出生时装备；开火请求由拥有者客户端经 `Server_Fire` 提交。
- 执行端：武器生成、弹药初始化与开火校验**仅服务器**；弹药状态与开火表现按拥有者／广播复制。
- 设计依据：[玩法设计第 4 节战斗数值与判定](../Design/01-gameplay.md#4-战斗数值与判定)、[服务端设计第 4 节操作请求与同步](../Design/03-server.md#4-操作请求与同步)、[总览 F05](../Design/00-overview.md#63-可认领的功能与模块清单)。

相关源码：

- `Source/Project_FPS/Battle/Data/BattleWeaponDefinition.h/.cpp`
- `Source/Project_FPS/Battle/Components/BattleWeaponComponent.h/.cpp`
- `Source/Project_FPS/Battle/BattleCharacter.h/.cpp`
- 复用的模板武器：`Source/Project_FPS/Variant_Shooter/Weapons/ShooterWeapon.h/.cpp`（未修改）

配套模块：[比赛状态](MatchState.md)、[玩家公共状态](PlayerState.md)、[基础战斗 HUD](BattleHUD.md)。

## 2. 设计约束（实现取舍）

| 事实 | 影响 |
| --- | --- |
| `AShooterWeapon::CurrentBullets` 非 `UPROPERTY`、不复制，且 `BeginPlay` 中直接填满 `MagazineSize` | 武器 Actor 自带的弹药**不具权威性**；本模块不向其写入弹药 |
| `AShooterWeapon` 没有任何 setter，`MagazineSize` 只能通过蓝图默认值配置 | 数据资产的 `MagazineSize`/`InitialReserveAmmo` 只写入 `UBattleWeaponComponent`，不改模板武器类 |
| 工作区决策 | 弹药唯一权威数据位于 `UBattleWeaponComponent` |

## 3. 数据资产 `UBattleWeaponDefinition`

`UPrimaryDataAsset`，服务端与客户端共享同一资产。资产实例需在编辑器中创建（C++ 类无默认资产）。

### 3.1 战斗数值（服务端权威）

| 字段 | 类型 | 默认值 | 范围／单位 | 说明 |
| --- | --- | --- | --- | --- |
| `DisplayName` | `FText` | 空 | — | 界面显示名 |
| `DamagePerShot` | `float` | `25.0` | `0..1000` | 单发伤害；首版不区分头部倍率 |
| `FireInterval` | `float` | `0.1` | `0.01..5` 秒 | 射击间隔 |
| `MagazineSize` | `int32` | `30` | `1..300` | 弹匣容量 |
| `InitialReserveAmmo` | `int32` | `90` | `0..600` | 出生备用弹药 |
| `ReloadDuration` | `float` | `2.0` | `0.1..10` 秒 | 换弹耗时（F05-05 使用） |
| `MaxRange` | `float` | `10000.0` | `100..100000` cm | 最大射击距离（F05-04 使用） |
| `bFullAuto` | `bool` | `true` | — | 是否全自动 |

数值基线取自[玩法设计](../Design/01-gameplay.md#4-战斗数值与判定)：生命 100、单发 25、间隔 0.1 秒、弹匣 30、备用 90、换弹 2 秒。

### 3.2 表现引用（客户端）

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `WeaponClass` | `TSoftClassPtr<AShooterWeapon>` | 空 | 要生成的武器 Actor 类；为空时不可装备 |
| `FirstPersonAnimInstanceClass` | `TSoftClassPtr<UAnimInstance>` | 空 | 第一人称动画蓝图 |
| `ThirdPersonAnimInstanceClass` | `TSoftClassPtr<UAnimInstance>` | 空 | 第三人称动画蓝图 |
| `FiringMontage` | `TSoftObjectPtr<UAnimMontage>` | 空 | 开火蒙太奇（F05-02 使用） |
| `FirstPersonWeaponSocket` | `FName` | `HandGrip_R` | 第一人称挂接插槽 |
| `ThirdPersonWeaponSocket` | `FName` | `HandGrip_R` | 第三人称挂接插槽 |

### 3.3 方法

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `IsEquippable() : bool` | 两端 | `WeaponClass` 非空即返回 true |

## 4. 武器组件 `UBattleWeaponComponent`

`USceneComponent`，作为出生装备的挂接点与权威弹药持有者。随角色复制。

### 4.1 复制属性

| 属性 | 类型 | 复制条件 | 写入方 | 实现状态 |
| --- | --- | --- | --- | --- |
| `MagazineAmmo` | `int32` | `COND_OwnerOnly` | 服务器 | F05-01 写入初始值 |
| `ReserveAmmo` | `int32` | `COND_OwnerOnly` | 服务器 | F05-01 写入初始值 |
| `bIsReloading` | `bool` | `COND_OwnerOnly` | [F05-05 预留] | 字段已复制，恒为 false |
| `ReloadStartServerTime` | `double` | `COND_OwnerOnly` | [F05-05 预留] | 字段已复制，恒为 0 |

`CurrentWeapon` 为 `Transient` 非复制字段。

### 4.2 接口

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `SetCurrentWeapon(AShooterWeapon*)` | 两端 | 记录当前武器引用 |
| `GetCurrentWeapon() : AShooterWeapon*` | 两端 | 纯读，无武器时为 nullptr |
| `InitializeAmmo(int32, int32)` | **仅服务器** | 写入弹匣与备用弹药；同时复位换弹状态；非权威调用直接返回 |
| `GetMagazineAmmo() : int32` | 两端 | 纯读权威弹匣弹药 |
| `GetReserveAmmo() : int32` | 两端 | 纯读权威备用弹药 |
| `IsReloading() : bool` | 两端 | 纯读换弹占位状态 |
| `GetReloadStartServerTime() : double` | 两端 | 纯读换弹开始时间占位 |

## 5. 初始化装配时序

| 步骤 | 位置 | 执行端 | 条件 | 行为 |
| --- | --- | --- | --- | --- |
| 1 | `ABattleCharacter` 构造函数 | 两端 | — | `bReplicates = true`；创建并挂接 `UBattleWeaponComponent` 到根组件 |
| 2 | `ABattleCharacter::BeginPlay` | 仅服务器 | `HasAuthority()` | 调用 `EquipFromDefinition()` |
| 3 | `EquipFromDefinition` | 仅服务器 | `WeaponDefinition` 非空且 `IsEquippable()`；当前无武器 | 同步加载 `WeaponClass`，调用 `AddWeaponClass` |
| 4 | `AddWeaponClass` | 仅服务器 | 非权威调用直接返回并记录警告 | 停用旧武器，`SpawnActor`（Owner/Instigator = 角色，`AlwaysSpawn`），记录到组件并 `ActivateWeapon` |
| 5 | `AShooterWeapon::BeginPlay`（引擎内） | 生成端 | — | 模板解析 `WeaponOwner` 并回调 `AttachWeaponMeshes(this)` |
| 6 | `AttachWeaponMeshes` | 生成端 | 武器有效 | 武器 Actor 挂到组件，第一／第三人称网格挂到角色网格插槽 |
| 7 | `OnWeaponActivated` | 生成端 | 武器与组件有效 | 服务器按 `MagazineSize`／`InitialReserveAmmo` 调用 `InitializeAmmo` 并复位换弹状态 |
| 8 | 属性复制 | 引擎 | 角色复制到客户端 | 客户端获得角色、组件与拥有者可见的弹药 |

**关键时序说明**：`SpawnActor` 会同步执行武器 `BeginPlay`，因此在 `AddWeaponClass` 内部，`AttachWeaponMeshes` 已先于 `SetCurrentWeapon` 执行；武器引用统一由 `AddWeaponClass` 与 `OnWeaponActivated` 提交，`AttachWeaponMeshes` 不做记录。

**重生配合**：`BeginPlay` 触发意味着 F06-04 销毁并重建角色后会自动重新装备并重置弹药，无需额外调用。

## 6. 为 F05-03／F05-04／F05-05 预留的接口

本阶段**不实现**任何开火、射线、伤害或换弹逻辑，仅预留数据与访问点：

| 任务 | 已预留的接入点 | 当前状态 |
| --- | --- | --- |
| F05-02 | `UBattleWeaponDefinition::FiringMontage`；`ABattleCharacter::PlayFiringMontage`／`AddWeaponRecoil` 空实现；`FireAction` 属性 | 未实现 |
| F05-03 | `UBattleWeaponComponent::GetMagazineAmmo()` 作为弹药校验入口；`ABattleCharacter::OnSemiWeaponRefire` 空实现；`SetupPlayerInputComponent` 中的绑定 TODO | 未实现；**未声明任何 RPC** |
| F05-04 | `UBattleWeaponComponent::MagazineAmmo` 作为唯一权威弹药；`ABattleCharacter::UpdateWeaponHUD`／`GetWeaponTargetLocation` 空实现；组件的世界变换可作为枪口与射线起点 | 未实现 |
| F05-05 | `UBattleWeaponComponent::bIsReloading`／`ReloadStartServerTime` 已复制；`UBattleWeaponDefinition::ReloadDuration` | 字段已就位，逻辑未实现；**无换弹 RPC** |

约束：

- F05-03／F05-05 的 RPC 必须声明在 C++ 类上并由服务器校验；客户端 Lua 不会在服务器执行。
- F05-04 的射线起点应基于服务器角色／武器组件变换，不得直接信任客户端坐标（[服务端设计第 5 节](../Design/03-server.md#5-战斗校验)）。
- 向 `MagazineAmmo`／`ReserveAmmo` 写入必须经服务器提交；`InitializeAmmo` 已做权威检查，后续修改弹药应复用同一检查口径。

## 7. F05-03 权威开火

### 7.1 实现状态

- 已实现：`Server_Fire` 请求入口、13 项校验链、权威扣弹、`Multicast_FireEffect` 表现通知（当前仅日志）、`PerformAuthoritativeShot` 空实现。
- **未实现**：射线检测与伤害提交（F05-04）、换弹（F05-05）、客户端本地预测与 ShotId 生成（F05-02）、LifeId 校验（F06）。
- 执行端：`Server_Fire` 只在服务器执行；`Multicast_FireEffect` 在服务器与所有客户端执行。

### 7.2 RPC 契约

#### `Server_Fire`（不可靠 Server RPC，定义于 `ABattleCharacter`）

| 参数 | 类型 | 含义与约束 |
| --- | --- | --- |
| `ClientRoundId` | `int32` | 客户端认为的当前 RoundId，须与 GameState 比对；`< 0` 在校验函数中拒绝 |
| `ClientLifeId` | `int32` | 客户端认为的当前 LifeId。**待 F06 接入后启用校验**，当前客户端传 `0`，服务器不校验该值；保留参数以固定签名 |
| `ShotId` | `int32` | 本生命期内单调递增的射击序号，用于去重；`< 0` 拒绝 |
| `AimViewOrigin` | `FVector_NetQuantize` | 客户端视点起点，仅作合理性参考；不作为射线起点 |
| `AimViewDir` | `FVector_NetQuantizeNormal` | 客户端瞄准方向，须接近单位长度 |

- 可靠性：**不可靠**（可丢包），依据[服务端设计](../Design/03-server.md#4-操作请求与同步)"单次步枪开火使用拥有者角色的 Server RPC，不可靠"。
- 权限：仅拥有者角色可提交；调用方必须是该 Pawn 的拥有者客户端。
- 校验函数 `Server_Fire_Validate` 只拦截会导致服务器异常的值（负数、NaN、超大坐标）；返回 `false` 会断开连接，因此行为级拒绝不使用它。

#### `Multicast_FireEffect`（不可靠 NetMulticast）

| 参数 | 类型 | 含义 |
| --- | --- | --- |
| `ShotId` | `int32` | 供拥有者客户端去重，避免本地预测与服务器事件重复播放 |
| `MuzzleLocation` | `FVector_NetQuantize` | 开火位置（当前为角色位置，F05-04 接入枪口 socket） |

当前实现只输出 `LogProject_FPS` 日志；表现接入属 F05-04／F05-02。

#### 调试命令（仅非 Shipping 构建）

`BattleTestFire [Count]`：`UFUNCTION(Exec)`，位于 **`ABattlePlayerController`**。

- 位置说明：`Exec` 必须放在 `PlayerController` 上，窗口控制台才会接收；`APawn`／`ACharacter` 不是控制台命令接收者（放在角色上只会回显命令名，不会执行）。
- 行为：先检查 `IsLocalController()`，再经 `GetPawn<ABattleCharacter>()` 取角色并调用 `ABattleCharacter::DebugTriggerFire()`；`Count` 限制在 `1..32`。
- `ABattleCharacter::DebugTriggerFire()` 生成递增 `ShotId`、取当前 `RoundId`、`ClientLifeId` 传 `0`，调用 `Server_Fire` 入口。
- 日志：全程使用 `LogTemp` 的 **`Warning`** 级别，确保控制台可见。关键日志点：
  - `TestFire: PC Called.`（PC 入口）
  - `DebugTriggerFire: Character called.`（角色入口）
  - `Server_Fire: Received on Server.`（服务器收到）
  - `Server_Fire: Fired! Ammo: N`（扣弹成功）
  - `fire rejected ... reason=...`（任一校验拒绝）
- 声明在头文件中无条件存在；实现由 `#if !UE_BUILD_SHIPPING` 包裹，Shipping 构建不提供函数体。
- 测试时须把焦点放在**客户端窗口**再按 `` ` ``／`~` 输入，命令只对本地客户端有意义。

### 7.3 服务端校验链

按顺序执行，任一失败即拒绝并返回，不扣弹、不广播：

| # | 校验 | 条件 | 拒绝原因码 |
| --- | --- | --- | --- |
| 1 | 权威归属 | `HasAuthority()` 且 `ABattlePlayerState`、`ABattleGameMode` 有效 | `NoAuthority` |
| 2 | RPC 级限流 | 距 `LastRpcServerTime` 小于 `MinRpcInterval`（默认 `0.05` 秒） | `RateLimited` |
| 3 | RoundId | `ClientRoundId == GameState.RoundId` | `StaleRound` |
| 4 | LifeId | **待 F06 接入后启用**；当前跳过 | — |
| 5 | 阶段 | `ABattleGameMode::CanAttack(PlayerState)` | `PhaseDenied` |
| 6 | 存活 | **占位为存活**，待 F06 提供权威存活状态 | `Dead`（当前不可达） |
| 7 | 武器可用 | `WeaponComponent` 与 `GetCurrentWeapon()` 有效 | `NoWeapon` |
| 8 | 换弹中 | `IsReloading()` 为真则拒绝；字段由 F05-05 写入，当前恒 `false` | `Reloading` |
| 9 | 弹药 | `GetMagazineAmmo() > 0` | `NoAmmo` |
| 10 | 射速 | `服务器世界时间 - LastFireServerTime >= FireInterval`（数据资产） | `RefireRate` |
| 11 | ShotId 去重 | `ShotId > LastAcceptedShotId`（同时拦截重复与乱序旧包） | `DuplicateShot` |
| 12 | 瞄准合理性 | `AimViewDir` 接近单位长度 | `BadAim` |
| 13 | 权威扣弹 | `ConsumeAmmo(1)`；失败按 `NoAmmo` 处理 | `NoAmmo` |

全部通过后依次：`RecordAcceptedShot(ShotId, ServerTime)` → `Multicast_FireEffect` → `PerformAuthoritativeShot`。

- 射速校验使用 `World->GetTimeSeconds()`（服务器世界时间），与比赛计时基准分离；依据[服务端设计](../Design/03-server.md#7-频率与运行预算)"输入限流与武器射速规则分别负责请求预算和玩法条件，不互相替代"。
- 拒绝日志使用 `Verbose` 级别，避免高频请求影响性能；不计入玩法统计。

### 7.4 权威扣弹

`UBattleWeaponComponent::ConsumeAmmo(int32 Amount)`：

- 仅服务器可调用，非权威直接返回 `false`。
- `Amount <= 0` 或 `MagazineAmmo < Amount` 返回 `false` 且不做修改。
- 成功时仅减少 `MagazineAmmo`；不修改 `ReserveAmmo`，不触发换弹（属 F05-05）。
- 结果通过既有 `COND_OwnerOnly` 复制给拥有者。

### 7.5 新增的服务器状态（不复制）

| 成员 | 类型 | 位置 | 说明 |
| --- | --- | --- | --- |
| `LastRpcServerTime` | `double` | `ABattleCharacter` | RPC 限流时间戳 |
| `MinRpcInterval` | `float` | `ABattleCharacter` | 限流最小间隔，默认 `0.05` 秒 |
| `LastFireServerTime` | `double` | `UBattleWeaponComponent` | 最近一次被接受的开火时间 |
| `LastAcceptedShotId` | `int32` | `UBattleWeaponComponent` | 已接受的最大 ShotId，`-1` 表示本生命期未开火 |

`InitializeAmmo` 会同时调用 `ResetFireState()`，保证新生命期重新开始射速与去重记录。

### 7.6 为 F05-04 预留的调用点

`ABattleCharacter::PerformAuthoritativeShot(int32 ShotId, const FVector& ViewOrigin, const FVector& ViewDir)`：

- `protected virtual`，当前为空实现，只有 `TODO(F05-04)` 注释。
- 在校验链全部通过、完成扣弹与 `Multicast_FireEffect` 之后调用一次。
- F05-04 只需在此函数内实现射线与伤害提交，**不修改 RPC 签名、不修改校验链**。

## 8. Lua 接入

M1／S3 阶段规则由 C++ 实现。Lua 规则接入点（开火、换弹、伤害决策）**未实现**，且需要先完成 Dedicated Server 上的 UnLua 运行验证，标注为**待验证**。

## 9. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 开火请求与本地预测（F05-02） | 未实现；`ShotId` 生成与输入绑定属 F05-02 |
| 权威开火校验与编号去重（F05-03） | **已实现**（见第 8 节）；LifeId 校验待 F06 |
| 服务器射线、命中、伤害提交、开火／命中反馈（F05-04） | 未实现；`PerformAuthoritativeShot` 为空实现 |
| 换弹开始／结束、弹药转移、中断（F05-05） | 未实现；`IsReloading` 只读占位恒为 `false` |
| 存活校验（F06） | 占位为存活，恒不拒绝 |
| `Multicast_FireEffect` 的实际表现 | 当前仅日志，无声音／动画／枪口效果 |
| 武器 Actor 自身是否复制 | **未决**，由 F05-04 决定 |
| 数据资产实例 | 需在编辑器创建（C++ 类无默认资产） |
| 双端装备与弹药复制 | 待验证 |
| Dedicated Server 上的装备与开火流程 | 待验证 |
| `BattleTestFire` 验证限流／去重／扣弹 | 待验证（仅非 Shipping 构建） |

## 10. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-15 | 新建 F05-01：数据资产、武器组件、角色复制开启、出生装备与权威弹药初始化 | 新增 `UBattleWeaponDefinition`、`UBattleWeaponComponent`；`ABattleCharacter` 新增 `WeaponDefinition`／`WeaponComponent`／`EquipFromDefinition`；无 RPC | C++ 编译与仓库文本检查待执行；双端装备与弹药复制待验证 |
| 2026-11-15 | 新增 F05-03 权威开火：`Server_Fire` 不可靠 RPC、13 项校验链、权威扣弹、`Multicast_FireEffect` 占位、`PerformAuthoritativeShot` 空实现、`BattleTestFire` 调试命令 | 新增 `Server_Fire`／`Multicast_FireEffect`／`Server_Fire_Validate`；`ConsumeAmmo`／`RecordAcceptedShot`／`ResetFireState`；`ClientLifeId` 参数已固定但暂不校验 | C++ 编译与仓库文本检查待执行；限流、去重、扣弹待 `BattleTestFire` 验证 |
| 2026-11-15 | 修正 `BattleTestFire` 的 `UFUNCTION` 预处理位置：声明移出 `#if`，实现保留在 `#if !UE_BUILD_SHIPPING` 中 | 对外接口不变；Shipping 构建与 Development 构建的可见性差异见 7.2 节 | UHT 编译错误修复，待重新编译验证 |
| 2026-11-15 | `BattleTestFire` 从 `ABattleCharacter` 移到 `ABattlePlayerController`（Exec 需挂在 PC 上才能被控制台接收）；新增 `ABattleCharacter::DebugTriggerFire()`；拒绝日志由 `Verbose` 提升为 `Warning` | 新增 `ABattlePlayerController::BattleTestFire`、`ABattleCharacter::DebugTriggerFire`；移除 `ABattleCharacter::BattleTestFire` | 待编译与 PIE 验证 |
