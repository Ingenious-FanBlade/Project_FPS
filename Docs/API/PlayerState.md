# 玩家公共状态接口（F08-01）

## 1. 模块职责与状态

本模块实现 `ABattlePlayerState`：正式对战中的玩家公共状态，保存并复制队伍、击杀与死亡计数，供记分板、敌我过滤与队伍识别读取。

- 实现状态：**M1 复制字段已实现**；`TeamId` 由 `SetTeam` 提交，`Kills`／`Deaths` 仅为复制字段。
- 未实现：击杀与死亡的写入逻辑（M2 F08-03 计分）、分队流程（M2 F08-02）、记分板展示（F09-02）。
- 调用方：F06-02（敌我过滤，**待与 F06 负责人对齐**）、F08-03（计分，M2）、F09-02（记分板）。
- 执行端：写入仅服务器；客户端只读取复制值。
- 设计依据：[服务端设计第 2 节状态归属](../Design/03-server.md#2-状态归属)（PlayerState：队伍、击杀、死亡、玩家显示信息）。

相关源码：

- `Source/Project_FPS/Battle/BattlePlayerState.h/.cpp`
- `Source/Project_FPS/Battle/BattleGameMode.cpp`（`PlayerStateClass` 绑定）

配套模块：[比赛状态](MatchState.md)、[基础战斗 HUD](BattleHUD.md)。

## 2. 队伍枚举

`EBattleTeamId`（`BattleTypes.h`）：

| 取值 | 数值 | 含义 |
| --- | --- | --- |
| `None` | 0 | 未分队；S1 默认值 |
| `Red` | 1 | 红队 |
| `Blue` | 2 | 蓝队 |

首版为两队对抗（[玩法设计对局规则](../Design/01-gameplay.md#2-对局规则)）。数值与 `ABattleGameState::TeamScores` 的下标对应，不得调整。

## 3. 复制属性

| 属性 | 类型 | 默认值 | 复制条件 | RepNotify | 实现状态 |
| --- | --- | --- | --- | --- | --- |
| `TeamId` | `EBattleTeamId` | `None` | `COND_None` | `OnRep_TeamId` | 字段与提交入口已实现；分队流程未实现（M2） |
| `Kills` | `int32` | `0` | `COND_None` | 无 | 仅复制字段；写入属 M2 F08-03 |
| `Deaths` | `int32` | `0` | `COND_None` | 无 | 仅复制字段；写入属 M2 F08-03 |

复制范围：所有客户端可见。S1 中 `Kills`／`Deaths` 恒为 0，客户端不得据此展示为"本局战绩已统计"。

## 4. 接口

| 接口 | 执行端 | 输入与输出 | 说明 |
| --- | --- | --- | --- |
| `GetTeam() : EBattleTeamId` | 两端 | 无入参 | 纯读，无副作用 |
| `SetTeam(EBattleTeamId)` | **仅服务器** | 目标队伍 | 非权威调用直接返回；`OnRep_TeamId` 当前没有内建订阅者 |

权威与约束：

- 队伍由服务器决定，客户端不能自定身份（[服务端设计第 3 节](../Design/03-server.md#3-比赛生命周期)）。
- `SetTeam` 不校验队伍人数均衡；均衡规则属 M2 F08-02。
- 击杀与死亡没有对外提交接口；M2 在 GameMode 内消费 F06 的唯一死亡事件后写入。

## 5. 生命周期

- PlayerState 由 `ABattleGameMode::PlayerStateClass` 在玩家进入时创建，玩家断开时销毁。
- 跨地图重载后重建，S1 不持久化任何字段。
- `OnRep_TeamId` 在客户端到达顺序上晚于 PlayerState 自身的复制；订阅方须容忍 `None` 初值。
- S1 不涉及 MatchId／RoundId 与 LifeId 关联，这些标识在 [比赛状态](MatchState.md) 与 F06 中维护。

## 6. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 分队与 `SetTeam` 的实际调用 | **未实现**（M2 F08-02） |
| 击杀／死亡计数写入 | **未实现**（M2 F08-03） |
| 记分板展示 | **未实现**（F09-02） |
| 敌我过滤读取 `GetTeam` | **未实现**（F06-02，待与 F06 负责人对齐） |
| 队伍复制与 RepNotify 联机表现 | **待验证**（需两个独立客户端） |

## 7. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-12 | 新建 F08-01 M1 玩家公共状态：队伍、击杀、死亡复制字段与 `OnRep_TeamId`、`SetTeam` 提交入口 | 新增 `ABattlePlayerState`；无客户端请求型 RPC | C++ 编译与仓库文本检查待执行；联机复制待验证 |
