// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BattleTypes.h"
#include "BattleGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBattleMatchPhaseChanged, EBattleMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleRoundIdentityChanged);

/**
 * 公共比赛状态（F08-01）。保存并复制 RoundId、比赛阶段与阶段截止时间。
 *
 * 权威归属：服务器写入，客户端只读取，不自行判定比赛结果。
 * 比分与队伍人数为 M2 预留字段，S1 恒定为零，写入逻辑属 M2（F08-03／F08-04）。
 *
 * 复制范围：所有客户端可见（COND_None，常驻复制）。
 * 客户端不通过本类发起任何请求；阶段变化由 GameMode 在服务器提交。
 */
UCLASS()
class PROJECT_FPS_API ABattleGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABattleGameState();

public:

	/** 当前对局标识。RoundId 在服务器实例内单调递增，每次重置后变化。 */
	UPROPERTY(ReplicatedUsing = OnRep_RoundIdentity, BlueprintReadOnly, Category = "Battle|Match")
	FBattleRoundIdentity RoundIdentity;

	/** 当前比赛阶段。客户端读取用于输入门控与界面显示。 */
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "Battle|Match")
	EBattleMatchPhase MatchPhase = EBattleMatchPhase::None;

	/**
	 * 当前阶段的服务器截止时间，单位为秒。
	 * 取值语义：GameState 未复制副本时表示沿用服务器世界时间的绝对秒数；
	 * 客户端以本地 Clock 计算剩余时间，不自行推进阶段。
	 */
	UPROPERTY(ReplicatedUsing = OnRep_PhaseEndTime, BlueprintReadOnly, Category = "Battle|Match")
	double PhaseEndServerTime = 0.0;

	/** [M2 预留] 两队得分，下标为 EBattleTeamId 的数值。S1 恒为 0。 */
	UPROPERTY(Replicated)
	int32 TeamScores[3] = { 0, 0, 0 };

	/** [M2 预留] 两队当前人数快照。S1 不维护，用于 M2 空队中止与记分板。 */
	UPROPERTY(Replicated)
	uint8 TeamPlayerCounts[3] = { 0, 0, 0 };

public:

	/** 阶段变化通知。两端均可订阅；客户端只用于展示与输入门控。 */
	UPROPERTY(BlueprintAssignable, Category = "Battle|Match")
	FBattleMatchPhaseChanged OnMatchPhaseChanged;

	/** 对局标识变化通知，客户端据此丢弃旧局回调。 */
	UPROPERTY(BlueprintAssignable, Category = "Battle|Match")
	FBattleRoundIdentityChanged OnRoundIdentityChanged;

public:

	/** 返回当前阶段。纯读，无副作用。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Match")
	EBattleMatchPhase GetMatchPhase() const { return MatchPhase; }

	/** 返回当前对局标识。纯读，无副作用。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Match")
	const FBattleRoundIdentity& GetRoundIdentity() const { return RoundIdentity; }

	/** 返回阶段截止时间（服务器秒）。纯读，无副作用。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Match")
	double GetPhaseEndServerTime() const { return PhaseEndServerTime; }

	/** [M2 预留] 读取指定队伍得分。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Score")
	int32 GetTeamScore(EBattleTeamId Team) const;

public:

	/** 服务器提交：设置阶段与截止时间，并广播阶段变化。执行端：仅服务器。 */
	void SetMatchPhase(EBattleMatchPhase NewPhase, double InPhaseEndServerTime);

	/** 服务器提交：设置新的对局标识并广播。执行端：仅服务器。 */
	void SetRoundIdentity(const FBattleRoundIdentity& InRoundIdentity);

protected:

	/** 已复制属性的客户端通知：对局标识变化。 */
	UFUNCTION()
	void OnRep_RoundIdentity();

	/** 已复制属性的客户端通知：阶段变化。 */
	UFUNCTION()
	void OnRep_MatchPhase();

	/** 已复制属性的客户端通知：阶段截止时间变化。 */
	UFUNCTION()
	void OnRep_PhaseEndTime();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
