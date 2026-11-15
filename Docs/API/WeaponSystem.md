# 武器系统接口（F05-01）

## 1. 模块职责与状态

本模块实现 F05-01 在 S3 第一阶段的范围：步枪配置数据资产、角色复制开启、出生装备与权威初始弹药。

- 实现状态：**F05-01 已实现**（数据资产定义、角色 `bReplicates`、服务器出生装备、权威弹药初始化）。
- **未实现**：本地开火与反馈（F05-02）、权威开火请求与校验（F05-03）、服务器射线／命中／伤害提交与开火反馈（F05-04）、换弹与中断（F05-05）。
- 调用方：服务器 `ABattleCharacter` 自身在出生时调用；客户端仅消费复制状态。
- 执行端：武器生成与弹药初始化**仅服务器**；弹药状态按拥有者复制。
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

## 7. Lua 接入

M1／S3 阶段规则由 C++ 实现。Lua 规则接入点（开火、换弹、伤害决策）**未实现**，且需要先完成 Dedicated Server 上的 UnLua 运行验证，标注为**待验证**。

## 8. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 开火请求与本地预测（F05-02） | 未实现 |
| 权威开火校验与编号去重（F05-03） | 未实现 |
| 服务器射线、命中、伤害提交、开火／命中反馈（F05-04） | 未实现 |
| 换弹开始／结束、弹药转移、中断（F05-05） | 未实现 |
| 武器 Actor 自身是否复制 | **未决**，由 F05-04 决定 |
| 数据资产实例 | 需在编辑器创建（C++ 类无默认资产） |
| 双端装备与弹药复制 | 待验证 |
| Dedicated Server 上的装备流程 | 待验证 |

## 9. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-15 | 新建 F05-01：数据资产、武器组件、角色复制开启、出生装备与权威弹药初始化 | 新增 `UBattleWeaponDefinition`、`UBattleWeaponComponent`；`ABattleCharacter` 新增 `WeaponDefinition`／`WeaponComponent`／`EquipFromDefinition`；无 RPC | C++ 编译与仓库文本检查待执行；双端装备与弹药复制待验证 |
