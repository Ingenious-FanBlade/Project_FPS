# 角色移动与行为限制接口（F04-04）

## 1. 模块职责与状态

本模块实现 F04-04 在 S3 范围内的服务端行为限制：冲刺状态与速度、冲刺与开火的互斥、以及依据比赛阶段对移动能力的基础门控。

- 实现状态：**F04-04 已实现**（冲刺状态复制、服务器权威速度、阶段移动门控、冲刺与开火互斥）。
- **未实现**：移动预测与远端平滑校正（F04-02）、死亡状态下的行为限制（F06）、结算与菜单阶段（M2 阶段流转）、体力／耐力系统。
- 调用方：客户端输入提交 `Server_SetSprint`；服务器 `ABattleGameMode::CanMove` 提供阶段许可。
- 执行端：冲刺状态、速度与移动禁用由**服务器**决定；客户端只做表现同步。
- 设计依据：[客户端设计第 2 节移动与第一人称表现](../Design/02-client.md#2-移动与第一人称表现)、[玩法设计第 3 节角色与操作](../Design/01-gameplay.md#3-角色与操作)、[服务端设计第 2 节状态归属](../Design/03-server.md#2-状态归属)。

相关源码：

- `Source/Project_FPS/Battle/BattleCharacter.h/.cpp`
- `Source/Project_FPS/Battle/BattleGameMode.h/.cpp`（`CanMove`，未改动）

配套模块：[比赛状态](MatchState.md)、[武器系统](WeaponSystem.md)。

## 2. 冲刺状态

### 2.1 复制属性

| 属性 | 类型 | 复制条件 | RepNotify | 用途 |
| --- | --- | --- | --- | --- |
| `bIsSprinting` | `bool` | `COND_None`（所有人可见） | `OnRep_IsSprinting` | 服务器权威冲刺状态；远端角色也需要该状态做表现 |

复制范围取 `COND_None` 而非 `COND_OwnerOnly`，因为其他客户端需要看到对手在冲刺。

### 2.2 速度

| 常量 | 默认值 | 类型 | 说明 |
| --- | --- | --- | --- |
| `WalkSpeed` | `300.0f` | `UPROPERTY(EditDefaultsOnly)` | 正常行走速度 |
| `SprintSpeed` | `600.0f` | `UPROPERTY(EditDefaultsOnly)` | 冲刺速度 |

**速度写入唯一入口**：`ApplySprintSpeed()` 根据 `bIsSprinting` 写 `CharacterMovement->MaxWalkSpeed`。

- 服务器：状态变化后主动调用。
- 客户端：`OnRep_IsSprinting` 与 `HandleMatchPhaseChanged` 中调用。
- 两端结果一致，避免"客户端自己改速度、服务器不认"的模板问题（Horror 模板即为此类反例）。

## 3. 接口

### `Server_SetSprint(bool bWantsSprint)`（**可靠** Server RPC）

| 参数 | 类型 | 含义 |
| --- | --- | --- |
| `bWantsSprint` | `bool` | 客户端请求的冲刺意图 |

- 可靠性：**可靠**。冲刺是低频开关事件，丢失会导致状态卡死（与开火"可丢"的语义不同）。
- 权限：仅拥有者客户端提交。

服务端校验（`_Implementation`）：

| # | 条件 | 结果 |
| --- | --- | --- |
| 1 | `HasAuthority()` | 否 → 直接返回 |
| 2 | `bWantsSprint == false` | **无条件接受**，保证任何校验失败后仍能恢复 |
| 3 | `ABattleGameMode::CanMove(PlayerState)` | 否 → 拒绝 |
| 4 | `WeaponComponent->IsReloading()` | 真 → 拒绝（与武器动作互斥） |
| 5 | 以上通过 | `bIsSprinting = true` 并 `ApplySprintSpeed()` |

### 查询与内部接口

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `IsSprinting() : bool` | 两端 | 纯读 |
| `ApplySprintSpeed()` | 两端 | 速度写入唯一入口 |
| `UpdateMovementPermission()` | **仅服务器** | 依据 `CanMove` 设置或解除移动禁用 |
| `HandleMatchPhaseChanged(EBattleMatchPhase)` | 两端 | 阶段变化回调；服务器额外更新移动许可 |

## 4. 移动能力门控

依据 `ABattleGameMode::CanMove`：

| 阶段 | 可移动 | 冲刺 |
| --- | --- | --- |
| `Waiting` | 是 | 允许 |
| `Countdown` | 否 | 拒绝 |
| `InProgress` | 是 | 允许 |
| `Ended`／`Aborted`／`Resetting`（M2） | 否 | 拒绝 |

服务器行为：

- **允许**：仅当 `MovementMode == MOVE_None` 时恢复 `MOVE_Walking`，避免打断空中状态；随后 `ApplySprintSpeed()`。
- **禁止**：强制取消冲刺、`StopMovementImmediately()` 并 `DisableMovement()`。不依赖客户端自觉。
- 无法取得 `ABattleGameMode` 或 `ABattlePlayerState` 时**不做限制**，避免初始化顺序导致误禁用。

触发时机（**不使用 Tick 轮询**，依据[服务端设计第 7 节](../Design/03-server.md#7-频率与运行预算)）：

1. `BeginPlay`（服务器）检查一次。
2. 订阅 `ABattleGameState::OnMatchPhaseChanged`，阶段变化时调用；`EndPlay` 解绑。

## 5. 冲刺与开火互斥

在 `Server_Fire_Implementation` 校验链中新增：

| 位置 | 校验 | 拒绝原因码 |
| --- | --- | --- |
| 第 11 步（射速之后、ShotId 去重之前） | `bIsSprinting == true` → 拒绝 | `Sprinting` |

- 放在去重之前，**不消耗 ShotId 编号**。
- M1 取舍：**拒绝**而非自动取消冲刺；玩家须先松开冲刺键再开火。
- 设计原文为「按下射击时结束冲刺」，本实现与其存在差异，见第 7 节。

## 6. 输入

| 动作 | 资产 | 绑定 |
| --- | --- | --- |
| `SprintAction` | 复用 `Content/Variant_Horror/Input/IA_Sprint` | `Started` → `Input_StartSprint`；`Completed` → `Input_StopSprint` |

- 资产在 `SetupPlayerInputComponent` 中按路径加载（方案②无蓝图可配置），加载失败打 `Warning` 且不绑定。
- 绑定方式与既有移动／跳跃输入一致。

## 7. 与设计文档的差异

| 设计描述 | 当前实现 | 说明 |
| --- | --- | --- |
| `02-client.md:52`「冲刺作为移动状态接入移动预测数据」 | 仅复制 `bIsSprinting` 并同步 `MaxWalkSpeed` | 属 F04-02 范围，未实现预测 |
| `01-gameplay.md:48`「冲刺与射击互斥，按下射击时结束冲刺」 | 冲刺中开火被**拒绝**，不会自动结束冲刺 | M1 取舍，服务端逻辑更简单；后续可改为自动取消 |
| 设计未规定数值 | `300`／`600` cm/s | 按需求给定，可在默认值中调整 |

## 8. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 移动预测、服务器校正与远端平滑（F04-02） | 未实现 |
| 死亡状态禁止移动／冲刺（F06） | 未实现；仅冲刺请求占位 |
| 结算／中止阶段的真实流转（M2） | 未实现；`CanMove` 已返回 false，但阶段尚不会进入 |
| 体力／耐力限制 | 未实现；设计未要求首版 |
| 冲刺表现（动画、音效、视场变化） | 未实现；属客户端 F04-03 |
| 双端速度同步与阶段门控 | 待验证 |

## 9. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-24 | 新建 F04-04：冲刺复制状态、可靠冲刺 RPC、服务器权威速度、阶段移动门控、冲刺与开火互斥 | 新增 `bIsSprinting`（复制）、`Server_SetSprint`、`IsSprinting`、`ApplySprintSpeed`、`UpdateMovementPermission`；开火校验链新增第 11 步与拒绝码 `Sprinting` | C++ 编译与仓库文本检查待执行；双端同步与门控待验证 |
