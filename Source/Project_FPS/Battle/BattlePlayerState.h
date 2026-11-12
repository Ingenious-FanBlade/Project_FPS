// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BattleTypes.h"
#include "BattlePlayerState.generated.h"

/**
 * 玩家公共状态（F08-01）。保存并复制队伍、击杀与死亡计数。
 *
 * 权威归属：服务器写入，客户端只读取。
 * S1 只提供复制字段并保持为零；击杀／死亡的写入逻辑属 M2（F08-03），
 * 队伍写入（分队）同属 M2（F08-02），S1 仅提供 SetTeam 供后续模块提交。
 */
UCLASS()
class PROJECT_FPS_API ABattlePlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABattlePlayerState();

public:

	/** 队伍编号。驱动敌我过滤（F06-02，待对齐）与队伍识别显示。 */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId, BlueprintReadOnly, Category = "Battle|Player")
	EBattleTeamId TeamId = EBattleTeamId::None;

	/** [M2 预留] 击杀数。S1 恒为 0。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Battle|Player")
	int32 Kills = 0;

	/** [M2 预留] 死亡数。S1 恒为 0。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Battle|Player")
	int32 Deaths = 0;

public:

	/** 返回队伍编号。纯读，无副作用。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Player")
	EBattleTeamId GetTeam() const { return TeamId; }

	/** 服务器提交队伍编号。执行端：仅服务器；M2 分队流程调用。 */
	void SetTeam(EBattleTeamId InTeamId);

protected:

	/** 已复制属性的客户端通知：队伍变化。 */
	UFUNCTION()
	void OnRep_TeamId();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
