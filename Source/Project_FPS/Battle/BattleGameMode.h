// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_FPSGameMode.h"
#include "BattleTypes.h"
#include "BattleGameMode.generated.h"

class ABattleCharacter;
class ABattleGameState;
class ABattlePlayerController;
class ABattlePlayerState;

/**
 * 正式对战 GameMode（F08-01 的服务器规则持有方）。
 *
 * 基类为 AProject_FPSGameMode，不继承 AShooterGameMode：后者携带模板队伍比分、
 * 本地 UI 创建和 PvE NPC 开关，与本模块的复制比赛状态冲突。
 *
 * S1 范围：M1 所需的阶段许可查询（CanAttack／CanRespawn／CanMove）、RoundId
 * 生成入口与阶段推进骨架 Waiting -> Countdown -> InProgress。
 * 比分、胜负、计分、结算、分队、匹配均属 M2。
 *
 * 本类不设 abstract：方案②以 PIE／命令行 URL 参数直接指定本 C++ 类，
 * 由引擎实例化。
 */
UCLASS(config = Game)
class PROJECT_FPS_API ABattleGameMode : public AProject_FPSGameMode
{
	GENERATED_BODY()

public:

	/** Constructor; binds the Battle GameState, PlayerState, PlayerController and Pawn classes. */
	ABattleGameMode();

public:

	/**
	 * 当前阶段是否允许该玩家攻击。
	 * 执行端：仅服务器。客户端不得据此改动本地权威状态。
	 * 本函数只判断阶段；存活、生命期与弹药由 F05-03／F06 叠加检查。
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Match")
	bool CanAttack(const ABattlePlayerState* InPlayerState) const;

	/**
	 * 当前阶段是否允许该玩家重生。执行端：仅服务器。
	 * 本函数只判断阶段；重生计时、安全出生点与生命期由 F06-04 负责。
	 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Match")
	bool CanRespawn(const ABattlePlayerState* InPlayerState) const;

	/** 当前阶段是否允许该玩家移动。执行端：仅服务器。 */
	UFUNCTION(BlueprintCallable, Category = "Battle|Match")
	bool CanMove(const ABattlePlayerState* InPlayerState) const;

	/** 返回当前复制的对局标识。两端可读。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Match")
	FBattleRoundIdentity GetRoundIdentity() const;

	/** 返回当前阶段。两端可读。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Match")
	EBattleMatchPhase GetMatchPhase() const;

protected:

	/** 游戏初始化：记录初始化完成，RoundId 与阶段在 PostInitializeComponents 提交。 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** GameState 就绪后建立第一局标识与初始阶段。 */
	virtual void PostInitializeComponents() override;

	/** 玩家进入：M1 直连模式下推进阶段骨架。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 返回 Battle GameState，未就绪时为 nullptr。 */
	ABattleGameState* GetBattleGameState() const;

	/** 生成新的 RoundId 并写入 GameState。M2 的 F08-05 在重置完成后调用。 */
	void AdvanceRoundId();

	/** 请求进入指定阶段并写入截止时间。执行端：仅服务器。 */
	void SetMatchPhase(EBattleMatchPhase NewPhase, double PhaseDurationSeconds);

	/** 倒计时完成回调：Countdown -> InProgress。 */
	void OnCountdownFinished();

	/** 返回当前服务器世界时间的秒数。 */
	double GetServerTimeSeconds() const;

protected:

	/** M1 直连联调：Waiting -> Countdown -> InProgress。M3 由匹配名单替代。 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Battle|M1 Debug")
	bool bAllowDirectConnectAutoStart = true;

	/** M1 直连模式下视为已达成的名单人数。M3 由匹配名单替代。 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Battle|M1 Debug")
	int32 DirectConnectExpectedPlayers = 2;

	/** 开局倒计时长度（秒），与玩法文档 5 秒基线一致。 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Battle|M1 Debug")
	float StartCountdownSeconds = 5.0f;

	/** 实例内递增的对局序号。 */
	int32 NextRoundId = 1;

	/** 倒计时计时器。 */
	FTimerHandle CountdownTimer;
};
