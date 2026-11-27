# 生命与重生接口（F06）

## 1. 模块职责与当前状态

本模块负责服务器权威生命、存活、伤害、死亡、重生与保护状态，以及客户端对这些复制状态的消费。本页描述当前实现和 F06-01 客户端接入所需的最小共享契约，不把后续规划写成可用能力。

- **已实现**：Battle HUD 在没有有效权威生命快照时显示 `-- / --`；既有 `UpdateBattleHUDHealth(CurrentHealth, MaxHealth)` 可直接显示有效生命。
- **待服务器接口对齐**：权威生命组件、复制字段、RepNotify／状态变化委托及 `ABattleCharacter` 生命组件访问器。
- **未实现**：`ABattlePlayerController` 对生命组件的正式绑定、伤害提交与规则、受伤事件、死亡、重生和保护。
- 调用方：未来由本地 `ABattlePlayerController` 读取当前 `ABattleCharacter` 的生命组件并刷新 HUD。
- 权威归属：生命与存活状态只允许服务器写入；客户端只读取复制结果并更新表现。
- 设计依据：[玩法设计第 5 节](../Design/01-gameplay.md#5-死亡与重生)、[客户端设计第 4 节](../Design/02-client.md#4-状态接收与界面)、[服务端设计第 2 节](../Design/03-server.md#2-状态归属)。

相关源码：

- `Source/Project_FPS/Battle/BattleCharacter.h/.cpp`
- `Source/Project_FPS/Battle/BattlePlayerController.h/.cpp`
- `Source/Project_FPS/Variant_Shooter/UI/BattleHUDWidget.h/.cpp`

## 2. F06-01 客户端最小契约

以下名称是客户端接入建议，须由服务器负责人确认或提供等价接口后才能作为已实现能力：

```cpp
class UBattleHealthComponent;

UBattleHealthComponent* ABattleCharacter::GetHealthComponent() const;

float UBattleHealthComponent::GetCurrentHealth() const;
float UBattleHealthComponent::GetMaxHealth() const;
bool UBattleHealthComponent::IsAlive() const;
FBattleLifeId UBattleHealthComponent::GetLifeId() const;

FOnBattleHealthStateChanged OnHealthStateChanged;
```

契约要求：

- 服务器是唯一写入方；拥有者客户端必须能读取生命、存活和当前 `FBattleLifeId`。
- 服务器本地提交和客户端 RepNotify 都通过同一个状态变化通知入口广播。
- 通知只表达“快照已变化”，订阅方随后通过只读访问器读取同一帧完整状态。
- 客户端不得通过该组件直接提交伤害、死亡、重生或保护状态。
- F06-01 客户端不根据 `CurrentHealth <= 0` 自行产生权威死亡；F06-03 以服务器 `bIsAlive` 和死亡状态为准。

## 3. 客户端绑定流程（待实现）

`ABattlePlayerController` 应沿用武器 HUD 的 Pawn 生命周期模式：

1. `BeginPlay`、`OnPossess` 和 `OnRep_Pawn` 调用统一刷新入口。
2. 从当前 `ABattleCharacter` 获取生命组件。
3. 订阅 `OnHealthStateChanged`，然后立即主动读取一次快照，覆盖 RepNotify 早于 Controller 绑定的情况。
4. 有效快照调用 `UpdateBattleHUDHealth(CurrentHealth, MaxHealth)`。
5. 无 Pawn、无生命组件、组件失效、`OnUnPossess`、Pawn 销毁或 Controller 结束时解除旧委托，并调用 `UpdateBattleHUDHealth(0, 0)` 清除旧生命。
6. 武器组件与生命组件分别判断有效性；生命组件尚未到达时不得破坏既有武器 HUD 绑定。

客户端保存组件弱引用，不复制或另存第二份权威生命、存活或 LifeId。

## 4. HUD 行为

| 输入 | 显示 | 含义 |
| --- | --- | --- |
| `MaxHealth > 0` 且参数有限 | `CurrentHealth / MaxHealth`，生命值限制到 `0..MaxHealth` | 有效服务器生命快照 |
| `MaxHealth <= 0` 或任一参数非有限 | `-- / --`，血条归零 | 当前没有有效权威生命快照 |

开发预览开启时，正式生命更新只写入 Lua 的正式状态缓存，不覆盖预览画面；关闭预览后恢复最新正式状态。

## 5. 后续模块边界

- F06-02：服务器负责友军、保护、已死亡目标等伤害规则；客户端只显示权威生命和受伤反馈。
- F06-03：服务器提交唯一死亡事件；客户端显示死亡并协调 F04／F05 停止本地动作。
- F06-04：服务器生成新 Pawn 和 LifeId；客户端解绑旧 Pawn、绑定新 Pawn，并恢复 HUD。
- F06-05：服务器决定保护是否生效或取消；客户端仅显示服务器保护状态和剩余时间。

## 6. 验证状态

| 项目 | 状态 |
| --- | --- |
| 无有效生命快照显示 `-- / --` | 代码已实现，UE 运行待验证 |
| 有效生命刷新 HUD | 既有显示入口已实现；权威组件接线待实现 |
| 预览关闭后恢复最新权威生命 | 既有 Lua 缓存流程已实现；权威组件接线后待验证 |
| Pawn 切换后的解绑与重绑 | 待实现 |
| 双客户端生命隔离与一致性 | 待服务器 F06-01 接口完成后验证 |

## 7. 变更记录

| 日期 | 变更 | 接口影响 | 验证状态 |
| --- | --- | --- | --- |
| 2026-11-25 | 建立 F06-01 客户端生命状态契约草案，并增加 HUD 无权威快照状态 | 既有 `UpdateBattleHUDHealth` 签名不变；约定 `(0, 0)` 表示清除旧生命 | UE 5.8.4 `Project_FPSEditor Win64 Development` 编译及 4 项 CI 单测通过；仓库检查仅因缺少 `luac5.4` 未通过；服务器接口、Controller 绑定和双客户端运行验证待完成 |
