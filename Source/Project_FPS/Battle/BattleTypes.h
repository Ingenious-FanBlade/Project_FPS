// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleTypes.generated.h"

/**
 * 团队编号。
 * 首版为两队对抗，数值固定写入复制属性，后续 M2／M3 不得插入或调整已有取值。
 */
UENUM(BlueprintType)
enum class EBattleTeamId : uint8
{
	None = 0,
	Red = 1,
	Blue = 2,
};

/**
 * 比赛阶段。
 *
 * S1（F08-01 M1）只使用 None／Waiting／Countdown／InProgress；
 * Ended／Aborted／Resetting 为 M2（F08-04、F08-05）预留，S1 不进入这些状态。
 * 枚举值一经提交不得插入、删除或调整顺序，否则复制序列化会与旧客户端不一致。
 */
UENUM(BlueprintType)
enum class EBattleMatchPhase : uint8
{
	/** 未初始化或当前没有有效对局。 */
	None = 0,

	/** 等待名单就绪；M1 直连模式下由 PostLogin 进入。 */
	Waiting = 1,

	/** 开局倒计时；S1 由 GameMode 计时推进到 InProgress。 */
	Countdown = 2,

	/** 对局中；S1 唯一允许攻击与重生的阶段。 */
	InProgress = 3,

	/** [M2 预留] 结算中，禁止攻击与重生。 */
	Ended = 4,

	/** [M2 预留] 对局中止。 */
	Aborted = 5,

	/** [M2 预留] 清场与地图重置。 */
	Resetting = 6,
};

/**
 * 一局的唯一标识。
 *
 * MatchId 在 M1 由 GameMode 生成本地 Guid 占位，M3 起由大厅／匹配服务注入；
 * RoundId 在服务器实例内单调递增，每次地图重置后重新生成。
 */
USTRUCT(BlueprintType)
struct FBattleRoundIdentity
{
	GENERATED_BODY()

	/** 本实例内的对局序号，从 1 开始递增，不跨实例比较。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Round")
	int32 RoundId = 0;

	/** 大厅分配的对局标识；M1 为本地生成的占位值，M3 起由匹配服务提供。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Round")
	FGuid MatchId;

	/** 服务器建立本局时的世界时间（秒），供客户端换算阶段剩余时间。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Round")
	double ServerStartTime = 0.0;

	bool IsValid() const { return RoundId > 0; }
};

/**
 * 一次出生（生命期）的标识。由 F06（生命、伤害、死亡与重生）负责生成与推进，
 * S1 只定义结构，不实现分配逻辑。
 *
 * 唯一键：M1／M2 为 (RoundId, LifeIndex)；M3 起扩展为 (MatchId, RoundId, LifeIndex)。
 * LifeIndex 在本局内单调递增且不复用，角色销毁后该生命期的请求全部失效。
 */
USTRUCT(BlueprintType)
struct FBattleLifeId
{
	GENERATED_BODY()

	/** 所属对局序号，必须与 GameState 当前 RoundId 一致。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Life")
	int32 RoundId = 0;

	/** 本局内的出生序号，首次出生为 1，每次重生加 1，不复用。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Life")
	int32 LifeIndex = 0;

	bool IsValid() const { return RoundId > 0 && LifeIndex > 0; }

	bool MatchesRound(const FBattleRoundIdentity& Round) const
	{
		return RoundId == Round.RoundId;
	}
};

/**
 * 一次权威射击的结果，用于 F05-04（射线命中）向 F06（伤害提交）传递参数。
 *
 * 跨模块契约：字段名与含义一旦被 F06 消费，不得随意改动；新增字段只追加。
 * 不复制、不持久化，只在服务器侧（PerformAuthoritativeShot 内）构造并传递。
 */
USTRUCT(BlueprintType)
struct FBattleShotResult
{
	GENERATED_BODY()

	/** 本次射击的客户端序号，用于关联同一次开火的表现与日志。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	int32 ShotId = 0;

	/** 射击发生时的对局序号，提交伤害时据此拒绝跨局结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	int32 RoundId = 0;

	/** 射击者。服务器侧对象，不参与复制。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	TObjectPtr<AActor> Shooter = nullptr;

	/** 被击中的战斗角色；未命中或命中非战斗角色时为 nullptr。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	TObjectPtr<AActor> Victim = nullptr;

	/** 被击中者的生命期序号。**待 F06 接入后填充**，当前恒为 0。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	int32 VictimLifeId = 0;

	/** 数据资产中的基础伤害，未经衰减或倍率处理。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	float BaseDamage = 0.0f;

	/** 本次射击的最终伤害。当前等于 BaseDamage（无距离衰减、无部位倍率）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	float FinalDamage = 0.0f;

	/** 是否命中可阻挡表面。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	bool bBlockingHit = false;

	/** 命中点（未命中时为射线终点）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	FVector HitLocation = FVector::ZeroVector;

	/** 命中骨骼名。[预留] 首版不区分头部倍率，F06 接入时按玩法文档决定是否使用。 */
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Shot")
	FName HitBoneName;

	/** 是否命中了可受伤的战斗角色（即 Victim 有效）。 */
	bool HasVictim() const { return Victim != nullptr; }
};
