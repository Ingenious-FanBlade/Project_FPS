# 比赛状态接口（F08-01）

## 1. 模块职责与状态

本模块实现 F08-01 在 M1 阶段的范围：公共比赛状态（GameState）、玩家公共状态（PlayerState）与 GameMode 阶段的持有者。服务器保存并复制状态，客户端只读取。

- 实现状态：**M1 最小实现**。RoundId、比赛阶段、阶段截止时间、队伍、击杀与死亡已声明并复制；阶段推进仅有 `Waiting → Countdown → InProgress` 骨架。
- 未实现：比分与胜负（F08-03）、结算与中止（F08-04）、清场与重置（F08-05）、分队（F08-02）、匹配名单（M3）。对应字段为 M2 预留，S1 恒定为零。
- 调用方：F04-04（行为限制）读取阶段；F05-03（权威开火）校验阶段；F06-04／F06-05（重生与保护，**待与 F06 负责人对齐**）查询阶段许可；F09 读取阶段与时间用于展示。
- 执行端：`ABattleGameMode`、`ABattleGameState`、`ABattlePlayerState` 的状态写入仅在服务器；客户端消费复制属性与 RepNotify。
- 设计依据：[服务端设计第 2 节状态归属](../Design/03-server.md#2-状态归属)、[服务端设计第 3 节比赛生命周期](../Design/03-server.md#3-比赛生命周期)、[总览 F08](../Design/00-overview.md#63-可认领的功能与模块清单)。

相关源码：

- `Source/Project_FPS/Battle/BattleTypes.h`
- `Source/Project_FPS/Battle/BattleGameMode.h/.cpp`
- `Source/Project_FPS/Battle/BattleGameState.h/.cpp`

配套模块：[玩家公共状态](PlayerState.md)、[基础战斗 HUD](BattleHUD.md)、[M1 与 M2 工作清单](../Tasks/05-m1-m2-checklist.md)。

## 2. 比赛阶段枚举

`EBattleMatchPhase`（`BattleTypes.h`），声明于 `UENUM(BlueprintType)`。

| 取值 | 数值 | M1 使用 | 含义 | 可移动 | 可攻击 | 可重生 |
| --- | --- | --- | --- | --- | --- | --- |
| `None` | 0 | 初始值 | 未初始化或没有有效对局 | 否 | 否 | 否 |
| `Waiting` | 1 | 是 | 等待名单就绪 | **是** | 否 | 否 |
| `Countdown` | 2 | 是 | 开局倒计时 | 否 | 否 | 否 |
| `InProgress` | 3 | 是 | 对局中 | 是 | 是 | 是 |
| `Ended` | 4 | 否（M2 预留） | 结算中 | 否 | 否 | 否 |
| `Aborted` | 5 | 否（M2 预留） | 对局中止 | 否 | 否 | 否 |
| `Resetting` | 6 | 否（M2 预留） | 清场与重置 | 否 | 否 | 否 |

约束：

- 枚举值一经发布不得插入、删除或调整顺序，否则复制序列化与旧客户端不一致。
- `Waiting` 允许移动与观察、禁止攻击与重生，依据[玩法设计第 3 节](../Design/01-gameplay.md#3-角色与操作)「死亡或比赛未开始时禁止攻击」。
- 阶段只由 `ABattleGameMode` 在服务器推进，客户端不得自行推进。

## 3. 对局标识

### `FBattleRoundIdentity`

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `RoundId` | `int32` | 实例内对局序号，从 1 开始单调递增，不跨实例比较 |
| `MatchId` | `FGuid` | 对局标识；M1 为本地生成的占位值，M3 起由大厅／匹配服务注入 |
| `ServerStartTime` | `double` | 服务器建立本局的世界时间（秒） |

生成规则：`ABattleGameMode::PostInitializeComponents` 建立第一局；`AdvanceRoundId()` 每次调用递增 `NextRoundId` 并写入 GameState。M1 不重置，M2 的 F08-05 在 `Resetting` 完成后调用 `AdvanceRoundId()`。

### `FBattleLifeId`（定义在 `BattleTypes.h`，分配逻辑属 F06，**待与 F06 负责人对齐**）

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `RoundId` | `int32` | 所属对局序号，必须等于 GameState 当前 `RoundId` |
| `LifeIndex` | `int32` | 本局内出生序号，首次为 1，每次重生加 1，不复用 |

唯一键：M1／M2 为 `(RoundId, LifeIndex)`；M3 起扩展为 `(MatchId, RoundId, LifeIndex)`。跨局时两者都变化，旧请求自然失效。

## 4. 复制属性

### `ABattleGameState`

| 属性 | 类型 | 复制条件 | RepNotify | 用途 |
| --- | --- | --- | --- | --- |
| `RoundIdentity` | `FBattleRoundIdentity` | `COND_None` | `OnRep_RoundIdentity` | 新局开始，订阅方丢弃旧局回调 |
| `MatchPhase` | `EBattleMatchPhase` | `COND_None` | `OnRep_MatchPhase` | 输入门控与界面显示 |
| `PhaseEndServerTime` | `double` | `COND_None` | `OnRep_PhaseEndTime` | 客户端按本地 Clock 计算剩余时间 |
| `TeamScores[3]` | `int32` | `COND_None` | 无 | **M2 预留**，S1 恒为 0 |
| `TeamPlayerCounts[3]` | `uint8` | `COND_None` | 无 | **M2 预留**，S1 不维护 |

下标对应 `EBattleTeamId` 的数值（0 = None，1 = Red，2 = Blue），因此数组长度为 3。

### `ABattlePlayerState`

| 属性 | 类型 | 复制条件 | RepNotify | 用途 |
| --- | --- | --- | --- | --- |
| `TeamId` | `EBattleTeamId` | `COND_None` | `OnRep_TeamId` | 敌我过滤与队伍识别 |
| `Kills` | `int32` | `COND_None` | 无 | **M2 预留**，S1 恒为 0 |
| `Deaths` | `int32` | `COND_None` | 无 | **M2 预留**，S1 恒为 0 |

## 5. 接口

### `ABattleGameState`

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `GetMatchPhase() : EBattleMatchPhase` | 两端 | 纯读 |
| `GetRoundIdentity() : const FBattleRoundIdentity&` | 两端 | 纯读 |
| `GetPhaseEndServerTime() : double` | 两端 | 纯读 |
| `GetTeamScore(EBattleTeamId) : int32` | 两端 | 纯读；M2 起有意义 |
| `OnMatchPhaseChanged`（动态多播，`EBattleMatchPhase`） | 两端 | 阶段变化；服务器提交时显式广播，客户端由 RepNotify 广播 |
| `OnRoundIdentityChanged`（动态多播，无参） | 两端 | 对局标识变化 |
| `SetMatchPhase(EBattleMatchPhase, double)` | **仅服务器** | 非权威调用直接返回，不广播 |
| `SetRoundIdentity(const FBattleRoundIdentity&)` | **仅服务器** | 非权威调用直接返回 |

### `ABattlePlayerState`

| 接口 | 执行端 | 说明 |
| --- | --- | --- |
| `GetTeam() : EBattleTeamId` | 两端 | 纯读 |
| `SetTeam(EBattleTeamId)` | **仅服务器** | 非权威调用直接返回；M2 分队流程调用 |

### `ABattleGameMode`

| 接口 | 执行端 | 语义 |
| --- | --- | --- |
| `CanAttack(const ABattlePlayerState*) const` | **仅服务器** | 阶段为 `InProgress` 时返回 true；**只判断阶段**，存活、生命期与弹药由 F05-03／F06 叠加 |
| `CanRespawn(const ABattlePlayerState*) const` | **仅服务器** | 阶段为 `InProgress` 时返回 true；**只判断阶段**，重生计时与出生点由 F06-04 负责 |
| `CanMove(const ABattlePlayerState*) const` | **仅服务器** | `Waiting` 与 `InProgress` 返回 true，其余为 false |
| `GetRoundIdentity() : FBattleRoundIdentity` | 两端 | 转发 GameState 的值 |
| `GetMatchPhase() : EBattleMatchPhase` | 两端 | 转发 GameState 的值 |

当前实现中三个 `Can*` 查询不读取 `InPlayerState`（S1 无队伍与存活信息），参数保留供 M2 加入队伍、阶段与空队判断后使用；签名不得改动。

### 阶段推进（服务器内部）

| 入口 | 触发 | 结果 |
| --- | --- | --- |
| `PostInitializeComponents` | 地图初始化 | `AdvanceRoundId()` 后进入 `Waiting` |
| `PostLogin` | 玩家进入且 `bAllowDirectConnectAutoStart` 为真、`GetNumPlayers() >= DirectConnectExpectedPlayers` | 进入 `Countdown`，写入截止时间并启动计时器 |
| `OnCountdownFinished` | 倒计时到期 | 进入 `InProgress` |

M1 不实现 `Ended`／`Aborted`／`Resetting` 的任何转换。

## 6. 配置

位于 `Config/DefaultGame.ini`：

```ini
[/Script/Project_FPS.BattleGameMode]
bAllowDirectConnectAutoStart=True
DirectConnectExpectedPlayers=2
StartCountdownSeconds=5.000000
```

| 配置 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| `bAllowDirectConnectAutoStart` | `bool` | `True` | M1 直连联调时自动推进到倒计时；M3 由匹配名单替代 |
| `DirectConnectExpectedPlayers` | `int32` | `2` | 直连模式下视为已达成的名单人数；联调基线为 2 人 |
| `StartCountdownSeconds` | `float` | `5.0` | 开局倒计时，与[玩法设计对局规则](../Design/01-gameplay.md#2-对局规则)的 5 秒基线一致 |

## 7. 激活方式

`ABattleGameMode` 不设 `abstract`，通过 PIE／命令行的 URL 参数直接指定，**不修改** `DefaultEngine.ini` 的全局 GameMode：

```text
?game=/Script/Project_FPS.BattleGameMode
```

- 编辑器：Play 设置中的 Additional Server Game Options 或地图 URL。
- 独立进程：`UnrealEditor.exe <项目> <地图>?game=/Script/Project_FPS.BattleGameMode -game`。
- 游戏内类：`ABattleCharacter`、`ABattlePlayerController`、`ABattleGameState`、`ABattlePlayerState` 由 GameMode 构造函数绑定，无需蓝图资产。

## 8. 生命周期与清理

- 对局标识在 `PostInitializeComponents` 生成；地图重载后服务器建立新 RoundId，客户端通过 `OnRep_RoundIdentity` 丢弃旧局回调。
- 玩家断开时其 `ABattlePlayerState` 由引擎销毁；S1 不维护队列、票据或跨局战绩。
- `OnMatchPhaseChanged` 在 Controller 与 GameState 销毁前须由订阅方解绑，避免旧回调。
- S1 不涉及 MatchId 有效期、入场资格与实例释放，这些属 M3。

## 9. 未实现与待验证

| 项 | 状态 |
| --- | --- |
| 阶段 `Ended`／`Aborted`／`Resetting` 的转换 | **未实现**（M2 F08-04／F08-05） |
| 比分、胜负、平局 | **未实现**（M2 F08-03） |
| 分队写入 `TeamId` | **未实现**（M2 F08-02） |
| 空队中止 | **未实现**（M2） |
| 地图重置与 RoundId 再生成 | **未实现**（M2 F08-05 调用 `AdvanceRoundId()`） |
| 匹配名单与入场就绪 | **未实现**（M3 F03） |
| `MatchId` 由匹配服务注入 | **未实现**（M3），M1 为本地 Guid |
| LifeId 分配 | **未实现**，结构已定义；**待与 F06 负责人对齐** |
| Lua 规则接入点 | **未实现**；M1 规则由 C++ 实现，Lua 接入待 DS 验证 |
| 复制与 RepNotify 的实际联机表现 | **待验证**（需两个独立客户端） |
| 阶段时间按客户端 Clock 换算的显示 | **待验证** |

## 10. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-12 | 新建 F08-01 M1 比赛状态：阶段枚举、RoundId、复制属性与 RepNotify、阶段许可查询、M1 直连联调配置 | 新增 4 个 Battle 类与上述接口；无客户端请求型 RPC | C++ 编译与仓库文本检查待执行；联机复制待验证 |
