# 武器系统接口（F05-01 / F05-03 / F05-04 / F05-05）

## 1. 模块职责与状态

本模块实现 F05 在 S3 范围内的服务端能力：步枪数据资产与出生装备、权威开火校验、服务器射线命中、换弹与中断。

- 实现状态：
  - **F05-01 已实现**：数据资产定义、角色 `bReplicates`、服务器出生装备、权威弹药初始化。
  - **F05-03 已实现**：`Server_Fire` 不可靠 RPC、13 项校验链、权威扣弹、表现通知占位。
  - **F05-04 已实现（部分）**：服务器权威射线与命中解析、`FBattleShotResult` 参数准备；伤害提交待 F06。
  - **F05-05 已实现**：`Server_Reload` 可靠 RPC、10 项校验链、服务端计时、弹药转移、中断接口。
- **未实现**：本地开火与换弹表现（F05-02）、伤害提交与友军过滤（F06）、LifeId 校验（F06）、死亡／比赛结束触发的换弹中断（F06／M2）。
- 调用方：服务器 `ABattleCharacter` 自身在出生时装备；开火与换弹请求由拥有者客户端经 RPC 提交。
- 执行端：武器生成、弹药初始化、开火校验、射线判定与换弹计时**仅服务器**；弹药状态与表现按拥有者／广播复制。
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
| `bIsReloading` | `bool` | `COND_OwnerOnly` | 服务器 | F05-05 在开始／完成／中断时写入 |
| `ReloadStartServerTime` | `double` | `COND_OwnerOnly` | 服务器 | 开始时写服务器时间，完成／中断时清零 |

`CurrentWeapon` 为 `Transient` 非复制字段。

### 4.2 接口

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `SetCurrentWeapon(AShooterWeapon*)` | 两端 | 记录当前武器引用 |
| `GetCurrentWeapon() : AShooterWeapon*` | 两端 | 纯读，无武器时为 nullptr |
| `InitializeAmmo(int32, int32)` | **仅服务器** | 写入弹匣与备用弹药；同时复位换弹状态；非权威调用直接返回 |
| `GetMagazineAmmo() : int32` | 两端 | 纯读权威弹匣弹药 |
| `GetReserveAmmo() : int32` | 两端 | 纯读权威备用弹药 |
| `IsReloading() : bool` | 两端 | 纯读权威换弹状态 |
| `GetReloadStartServerTime() : double` | 两端 | 纯读权威换弹开始服务器时间 |
| `OnWeaponStateChanged` | 服务器提交端、拥有者客户端 | 任一上述复制状态变化时广播；订阅者随后通过只读接口取得完整快照 |

四个拥有者可见字段使用同一个 `OnRep_WeaponState`。一次网络更新可能触发多次通知，因此订阅方必须按“状态已变化”处理，不得按通知次数累计弹药或进度。

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
| 8 | 属性复制 | 引擎 | 角色复制到客户端 | 拥有者客户端获得弹药与换弹状态，RepNotify 广播 `OnWeaponStateChanged` |
| 9 | HUD 绑定 | 拥有者客户端 | `ABattlePlayerController` 已取得 Battle Pawn | 绑定组件通知并立即读取一次完整快照；Pawn 切换或销毁时解绑 |

**关键时序说明**：`SpawnActor` 会同步执行武器 `BeginPlay`，因此在 `AddWeaponClass` 内部，`AttachWeaponMeshes` 已先于 `SetCurrentWeapon` 执行；武器引用统一由 `AddWeaponClass` 与 `OnWeaponActivated` 提交，`AttachWeaponMeshes` 不做记录。

**重生配合**：`BeginPlay` 触发意味着 F06-04 销毁并重建角色后会自动重新装备并重置弹药，无需额外调用。

## 6. F05 子任务接入状态

| 任务 | 主要接入点 | 当前状态 |
| --- | --- | --- |
| F05-02 | `UBattleWeaponDefinition::FiringMontage`；`ABattleCharacter::PlayFiringMontage`／`AddWeaponRecoil` 空实现；`FireAction` 属性 | 未实现 |
| F05-03 | `Server_Fire`、权威弹药校验与扣减、ShotId 去重 | 已实现；LifeId 校验待 F06 |
| F05-04 | 服务器射线、`FBattleShotResult`、拥有者弹药 HUD 刷新 | 射线和 HUD 已实现；伤害提交待 F06 |
| F05-05 | `Server_Reload`、权威计时与弹药转移、拥有者换弹 HUD | 已实现；死亡／比赛结束中断触发分别待 F06／F08 |

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

**RoundId 来源约束（客户端）**：`ClientRoundId` 必须取自复制的 `ABattleGameState::GetRoundIdentity().RoundId`。

- **不得从 GameMode 读取**：`AGameModeBase` 只存在于服务器，客户端 `GetWorld()->GetAuthGameMode()` 恒为 `nullptr`，读到的会是默认值 0，导致服务端按 `StaleRound` 拒绝。
- 获取方式：`GetWorld()->GetGameState<ABattleGameState>()`，判空后再读 `RoundIdentity.RoundId`。
- 若 GameState 尚未同步（值为 0），客户端应**中止本次请求并提示等待**，不得发送 0 占位值。
- 该约束同样适用于 F05-02 客户端本地射击与后续任何携带 RoundId 的请求。

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
| 11 | **冲刺互斥** | `bIsSprinting == true` 则拒绝（F04-04） | `Sprinting` |
| 12 | ShotId 去重 | `ShotId > LastAcceptedShotId`（同时拦截重复与乱序旧包） | `DuplicateShot` |
| 13 | 瞄准合理性 | `AimViewDir` 接近单位长度 | `BadAim` |
| 14 | 权威扣弹 | `ConsumeAmmo(1)`；失败按 `NoAmmo` 处理 | `NoAmmo` |

第 11 步放在射速之后、去重之前，**不消耗 ShotId 编号**；冲刺规则见[角色移动与行为限制](CharacterMovement.md)。

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

### 7.6 `PerformAuthoritativeShot` 调用点

`ABattleCharacter::PerformAuthoritativeShot(int32 ShotId, const FVector& ViewOrigin, const FVector& ViewDir)`：

- `protected virtual`，在校验链全部通过、完成扣弹与 `Multicast_FireEffect` 之后调用一次。
- F05-04 在此实现射线检测；**不修改 RPC 签名、不修改校验链**。详见第 8 节。

## 8. F05-04 射线命中与反馈

### 8.1 实现状态

- 已实现：服务器权威射线、命中解析、`FBattleShotResult` 参数准备、诊断日志。
- **未实现**：伤害提交（F06）、友军过滤（F06-02）、距离衰减、部位倍率、命中反馈 Multicast（F09-03）。

### 8.2 射线参数

| 项 | 取值 | 说明 |
| --- | --- | --- |
| 起点 | 服务器第一人称相机位置；无相机时退化为角色位置 | **不采用客户端 `AimViewOrigin`**，依据[服务端设计](../Design/03-server.md#5-战斗校验)"射击起点由服务器构造，不能直接信任客户端任意坐标" |
| 方向 | 控制器 `GetControlRotation()`；无控制器时用角色朝向 | **不采用客户端 `AimViewDir`** |
| 终点 | `起点 + 方向 * MaxRange` | `MaxRange` 取自 `UBattleWeaponDefinition`，缺失时回退 `10000` cm |
| 通道 | `ECC_Visibility` | 项目无自定义 `Weapon` 通道；`Projectile` 通道（`ECC_GameTraceChannel1`）为 `bTraceType=False`，不能用于射线 |
| 忽略 | `AddIgnoredActor(this)` | 忽略射击者自身 |
| 查询参数 | `SCENE_QUERY_STAT(BattleAuthoritativeShot)` | 便于性能分析定位 |

客户端传入的 `AimViewOrigin`／`AimViewDir` 只在校验链第 12 步做合理性检查，**不参与命中判定**。

### 8.3 命中解析

| 命中对象 | 判定 | 处理 |
| --- | --- | --- |
| `ABattleCharacter`（其他玩家） | `Cast<ABattleCharacter>` 成功且非自身 | 填充 `FBattleShotResult`，记录日志，TODO 提交 F06 |
| 非战斗 Actor（环境、道具） | 转换失败 | 只记日志，不产生伤害 |
| 无命中 | `!bBlockingHit` | 记日志"no hit" |

命中其他玩家时日志包含：`ShotId`、`RoundId`、受害者名、双方 `TeamId`、伤害值、命中点、骨骼名。

### 8.4 `FBattleShotResult`（定义于 `BattleTypes.h`）

跨 F05／F06 的通信契约，不复制、不持久化，只在服务器侧构造并传递。字段见[比赛状态](MatchState.md)类型章节或源码 `BattleTypes.h`：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ShotId` | `int32` | 射击序号，关联表现与日志 |
| `RoundId` | `int32` | 射击发生时的对局序号 |
| `Shooter` | `AActor*` | 射击者 |
| `Victim` | `AActor*` | 被击中的战斗角色，未命中为 `nullptr` |
| `VictimLifeId` | `int32` | **待 F06 填充**，当前恒 `0` |
| `BaseDamage` | `float` | 数据资产基础伤害 |
| `FinalDamage` | `float` | 最终伤害，当前 = `BaseDamage` |
| `bBlockingHit` | `bool` | 是否命中可阻挡表面 |
| `HitLocation` | `FVector` | 命中点；未命中时为射线终点 |
| `HitBoneName` | `FName` | 命中骨骼名，[预留] 首版不区分倍率 |

### 8.5 为 F06 预留的伤害提交点

`PerformAuthoritativeShot` 末尾保留：

```cpp
// TODO(F06): 在此调用 F06 的统一伤害提交接口，参数为 Result。
```

并输出 `Warning` 级日志 `TODO(F06) damage submission not implemented; no health changed.`。

F06 接入要求：

- 伤害入口负责**存活校验、重生保护、友军过滤、死亡唯一结算**，F05 不重复实现这些规则。
- `Result.RoundId` 用于拒绝跨局迟到的伤害提交。
- `Result.VictimLifeId` 需在 F06 提供 LifeId 后填充，用于拒绝旧生命期的伤害。

### 8.6 反馈

`Multicast_FireEffect` 保持现状（日志占位），枪口、声音与开火动画的表现接入属 F05-02。命中反馈属 F09-03，依赖 F06 的权威伤害结果，本阶段不新增。

## 9. F05-05 换弹与中断

### 9.1 实现状态

- 已实现：`Server_Reload` 可靠 RPC、10 步校验链、服务端计时、弹药转移、中断接口 `InterruptReload`、`Multicast_ReloadEffect` 日志占位。
- **未实现**：换弹动画与音效（F05-02）、死亡中断的实际触发（F06）、比赛结束中断（M2 阶段流转）。拥有者 HUD 进度已由 F09 接入。

### 9.2 RPC 契约

#### `Server_Reload`（**可靠** Server RPC，定义于 `ABattleCharacter`）

| 参数 | 类型 | 含义与约束 |
| --- | --- | --- |
| `ClientRoundId` | `int32` | 客户端认为的当前 RoundId，须与 GameState 比对；来源约束同 7.2 节 |
| `ClientLifeId` | `int32` | **待 F06 接入后启用校验**，当前传 `0`、不校验 |
| `ReloadRequestId` | `int32` | 本生命期内递增的换弹请求编号，用于幂等去重；`< 0` 在校验函数中拒绝 |

- 可靠性：**可靠**（`Reliable`）。与开火不同，依据[服务端设计](../Design/03-server.md#4-操作请求与同步)"换弹、投掷请求：低频可靠 Server RPC"。
- 幂等：`ReloadRequestId <= LastAcceptedReloadId` 时**静默丢弃**，不记录拒绝日志；可靠 RPC 的重复投递属正常现象。
- 校验函数 `Server_Reload_Validate` 只拦截负数；返回 `false` 会断连。

#### `Multicast_ReloadEffect`（不可靠 NetMulticast）

| 参数 | 类型 | 含义 |
| --- | --- | --- |
| `ReloadRequestId` | `int32` | 客户端去重 |
| `bStarted` | `bool` | `true` = 开始，`false` = 完成或中断 |
| `ServerStartTime` | `double` | 开始时的服务器时间；完成／中断时为 `0` |

当前只输出日志；动画与进度插值属 F05-02。

#### 调试命令（非 Shipping 构建）

`BattleTestReload`：位于 `ABattlePlayerController`，经 `GetPawn<ABattleCharacter>()->DebugTriggerReload()` 发送一次换弹请求，`ReloadRequestId` 按内存递增。

### 9.3 服务端校验链

| # | 校验 | 条件 | 拒绝原因码 |
| --- | --- | --- | --- |
| 1 | 权威归属 | `HasAuthority()` 且 `PlayerState`／`GameMode` 有效 | `NoAuthority` |
| 2 | 幂等去重 | `ReloadRequestId <= LastAcceptedReloadId` → **静默丢弃** | — |
| 3 | RoundId | `ClientRoundId == GameState.RoundId` | `StaleRound` |
| 4 | LifeId | **待 F06 启用**，当前跳过 | — |
| 5 | 阶段 | `ABattleGameMode::CanAttack(PlayerState)` | `PhaseDenied` |
| 6 | 存活 | 占位为存活（F06 未实现） | `Dead`（不可达） |
| 7 | 武器可用 | `WeaponComponent` 与当前武器有效 | `NoWeapon` |
| 8 | 已在换弹 | `IsReloading()` 为真 | `AlreadyReloading` |
| 9 | 弹匣已满 | `MagazineAmmo >= MagazineSize` | `MagazineFull` |
| 10 | 备用弹药 | `ReserveAmmo <= 0` | `NoReserveAmmo` |

拒绝原因码在 7.3 节枚举基础上**追加** `AlreadyReloading`／`MagazineFull`／`NoReserveAmmo`，既有值未改动。

**不实现主动取消**：换弹中再次请求被 `AlreadyReloading` 拒绝；换弹只能自然完成或被外部中断。

### 9.4 计时与弹药转移

```text
校验通过
  → WeaponComponent->BeginReload(ServerTime)     // bIsReloading=true, ReloadStartServerTime
  → WeaponComponent->RecordAcceptedReload(id)
  → ReloadGeneration++，SetTimer(ReloadDuration)  // 时长取数据资产，最小 0.1 秒

计时到期 OnReloadCompleted(Generation)
  → Generation != ReloadGeneration 时丢弃（旧计时器）
  → NeededAmount = MagazineSize - MagazineAmmo
  → LoadAmount   = Min(NeededAmount, ReserveAmmo)
  → WeaponComponent->CompleteReload(LoadAmount)  // 转移并清零换弹状态
```

- 采用 `TimerManager` 而非 Tick，依据[服务端设计](../Design/03-server.md#7-频率与运行预算)"比赛计时采用服务器时间，不能依赖固定 Tick 次数累加"。
- 转移规则：**换满为止**；备弹不足时抽干备弹。首次换弹后基线为 `30 / 60`。

### 9.5 中断

`ABattleCharacter::InterruptReload(const FString& Reason)`，执行端：仅服务器。

- 递增 `ReloadGeneration` 使排队中的完成回调失效，并 `ClearTimer`。
- 调用 `WeaponComponent->CancelReload()`：清除换弹状态，**不转移任何弹药**（原子性）。
- 广播 `Multicast_ReloadEffect(..., bStarted=false, ...)`。

| 触发源 | 状态 |
| --- | --- |
| 角色 `EndPlay`（销毁、重生、切图） | **已实现** |
| 死亡 | **未实现**，待 F06 调用 `InterruptReload` |
| 比赛结束／阶段离开 `InProgress` | **未实现**，待 M2 阶段流转通知 |

### 9.6 与开火的关系

- 开火校验链第 8 步已拦截 `IsReloading()`，**未修改开火链**。
- `UBattleWeaponComponent::ConsumeAmmo` 新增防御：换弹期间返回 `false`，保证绕过校验链的调用也不会扣弹。

### 9.7 新增的服务器状态（不复制）

| 成员 | 类型 | 位置 | 说明 |
| --- | --- | --- | --- |
| `LastAcceptedReloadId` | `int32` | `UBattleWeaponComponent` | 已接受的最大换弹请求编号，`-1` 表示本生命期未换弹 |
| `ReloadGeneration` | `int32` | `ABattleCharacter` | 计时器代次，防旧计时器误触发 |
| `ReloadTimer` | `FTimerHandle` | `ABattleCharacter` | 换弹计时器 |
| `ActiveReloadRequestId` | `int32` | `ABattleCharacter` | 当前换弹请求编号，用于日志与 Multicast |

## 10. Lua 接入

M1／S3 阶段规则由 C++ 实现。Lua 规则接入点（开火、换弹、伤害决策）**未实现**，且需要先完成 Dedicated Server 上的 UnLua 运行验证，标注为**待验证**。

## 11. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 开火请求与本地预测（F05-02） | 未实现；`ShotId` 生成与输入绑定属 F05-02 |
| 权威开火校验与编号去重（F05-03） | **已实现**（见第 7 节）；LifeId 校验待 F06 |
| 服务器射线与命中解析（F05-04） | **已实现**（见第 8 节）；起点与方向均取服务器数据 |
| 伤害提交（F05-04 → F06） | **未实现**；`FBattleShotResult` 已备好，`PerformAuthoritativeShot` 末尾为 TODO 占位 |
| 友军过滤、距离衰减、部位倍率 | 未实现；属 F06-02 与玩法数值，F05 不重复实现 |
| 命中反馈 Multicast | 未实现；属 F09-03，依赖 F06 权威伤害结果 |
| `VictimLifeId` 填充 | 未实现；待 F06 提供 LifeId |
| 换弹开始／结束、弹药转移、中断（F05-05） | **已实现**（见第 9 节）；主动取消刻意不实现 |
| 换弹动画与音效 | 未实现；属 F05-02 |
| 拥有者 HUD 弹药与换弹进度 | **已实现代码接入**；2026-11-25 用户确认弹匣与备用弹药的双客户端 `COND_OwnerOnly` 隔离有效；换弹状态与进度、开火扣弹及换弹转移显示仍待分别验证 |
| 死亡触发的换弹中断 | 未实现；`InterruptReload` 已提供，待 F06 调用 |
| 比赛结束触发的换弹中断 | 未实现；待 M2 阶段流转通知 |
| 存活校验（F06） | 占位为存活，恒不拒绝 |
| `Multicast_FireEffect` 的实际表现 | 当前仅日志，无声音／动画／枪口效果 |
| `Multicast_ReloadEffect` 的实际表现 | 当前仅日志，无换弹动作 |
| 武器 Actor 自身是否复制 | 保持不复制；玩家弹道改为服务器射线后，武器 Actor 仅承担表现 |
| 数据资产实例 | 需在编辑器创建（C++ 类无默认资产） |
| 双端装备与弹药复制 | 部分验证：弹匣与备用弹药的双客户端拥有者隔离有效；装备表现及完整状态变化仍待验证 |
| Dedicated Server 上的装备与开火流程 | 待验证 |
| `BattleTestFire` 验证限流／去重／扣弹 | **已验证**（Development 构建，双端 PIE） |
| 射线命中日志与实际命中结果 | 待验证 |
| `BattleTestReload` 验证换弹、计时与弹药转移 | 待验证 |

## 12. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-15 | 新建 F05-01：数据资产、武器组件、角色复制开启、出生装备与权威弹药初始化 | 新增 `UBattleWeaponDefinition`、`UBattleWeaponComponent`；`ABattleCharacter` 新增 `WeaponDefinition`／`WeaponComponent`／`EquipFromDefinition`；无 RPC | C++ 编译与仓库文本检查待执行；双端装备与弹药复制待验证 |
| 2026-11-15 | 新增 F05-03 权威开火：`Server_Fire` 不可靠 RPC、13 项校验链、权威扣弹、`Multicast_FireEffect` 占位、`PerformAuthoritativeShot` 空实现、`BattleTestFire` 调试命令 | 新增 `Server_Fire`／`Multicast_FireEffect`／`Server_Fire_Validate`；`ConsumeAmmo`／`RecordAcceptedShot`／`ResetFireState`；`ClientLifeId` 参数已固定但暂不校验 | C++ 编译与仓库文本检查待执行；限流、去重、扣弹待 `BattleTestFire` 验证 |
| 2026-11-15 | 新增 F05-04 服务器权威射线与命中解析：`GetServerShotOrigin`／`GetServerShotDirection`、`ECC_Visibility` 单线检测、`FBattleShotResult` 参数准备、F06 伤害提交占位；`FBattleShotResult` 定义于 `BattleTypes.h` | 新增 `FBattleShotResult`（跨 F05／F06 契约，未复制）；`ABattleCharacter` 新增两个 protected 查询函数；`PerformAuthoritativeShot` 由空实现变为射线实现；RPC 签名与校验链不变 | C++ 编译与仓库文本检查待执行；命中日志待验证 |
| 2026-11-15 | 新增 F05-05 换弹与中断：`Server_Reload` 可靠 RPC、10 项校验链、TimerManager 计时、换满为止的弹药转移、`InterruptReload`、`Multicast_ReloadEffect`、`BattleTestReload` 调试命令；`ConsumeAmmo` 增加换弹期防御 | 新增 `Server_Reload`／`Multicast_ReloadEffect`／`Server_Reload_Validate`／`InterruptReload`；组件新增 `BeginReload`／`CompleteReload`／`CancelReload`／`RecordAcceptedReload`／`GetLastAcceptedReloadId`；拒绝原因码追加三项；开火链未改动 | C++ 编译与仓库文本检查待执行；换弹与中断待 `BattleTestReload` 验证 |
| 2026-11-24 | F04-04 在开火校验链第 11 步新增冲刺互斥（射速后、去重前），拒绝码 `Sprinting`；校验链由 13 项扩为 14 项 | 新增拒绝原因 `Sprinting`；开火链新增一步，其余顺序不变；冲刺契约见 [CharacterMovement.md](CharacterMovement.md) | C++ 编译与仓库文本检查待执行；冲刺中开火拒绝待验证 |
| 2026-11-15 | 补充 `ClientRoundId` 来源约束：必须取自复制的 `ABattleGameState`，不得从 GameMode 读取 | 契约说明补充，无代码变更 | F05-03 双端验证通过（RoundId=1 双方一致、扣弹正常、Multicast 双端接收） |
| 2026-11-15 | `BattleTestFire` 从 `ABattleCharacter` 移到 `ABattlePlayerController`（Exec 需挂在 PC 上才能被控制台接收）；新增 `ABattleCharacter::DebugTriggerFire()`；拒绝日志由 `Verbose` 提升为 `Warning` | 新增 `ABattlePlayerController::BattleTestFire`、`ABattleCharacter::DebugTriggerFire`；移除 `ABattleCharacter::BattleTestFire` | 待编译与 PIE 验证 |
| 2026-11-17 | 将拥有者可见弹药与换弹复制状态接入正式 Battle HUD | 四个状态字段改为统一 RepNotify；新增 `OnWeaponStateChanged`、`ABattleCharacter::GetWeaponReloadDuration()`；不改变服务器写入权威和现有 RPC | UE 5.8 `Project_FPSEditor Win64 Development` 编译通过；双端 PIE 待验证 |
| 2026-11-25 | 补充拥有者可见弹药的双客户端隔离验证记录 | 对外契约不变 | 用户确认弹匣与备用弹药在双客户端之间保持 `COND_OwnerOnly` 隔离；换弹状态与进度、开火扣弹、换弹转移、装备表现和 Dedicated Server 路径不在本次验证范围内 |
