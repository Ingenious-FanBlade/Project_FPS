// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleGameState.h"
#include "Net/UnrealNetwork.h"
#include "Project_FPS.h"

ABattleGameState::ABattleGameState()
{
	// 比赛状态需要复制到所有客户端。
	bReplicates = true;
}

int32 ABattleGameState::GetTeamScore(EBattleTeamId Team) const
{
	// TeamScores 为定长数组，下标与 EBattleTeamId 数值对应，越界返回 0。
	const int32 Index = static_cast<int32>(Team);
	return (Index >= 0 && Index < UE_ARRAY_COUNT(TeamScores)) ? TeamScores[Index] : 0;
}

void ABattleGameState::SetMatchPhase(EBattleMatchPhase NewPhase, double InPhaseEndServerTime)
{
	// 只有服务器可以提交比赛阶段。
	if (!HasAuthority())
	{
		return;
	}

	MatchPhase = NewPhase;
	PhaseEndServerTime = InPhaseEndServerTime;

	// 服务器自身不会收到 RepNotify，需要显式广播一次。
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void ABattleGameState::SetRoundIdentity(const FBattleRoundIdentity& InRoundIdentity)
{
	// 只有服务器可以提交对局标识。
	if (!HasAuthority())
	{
		return;
	}

	RoundIdentity = InRoundIdentity;
	OnRoundIdentityChanged.Broadcast();
}

void ABattleGameState::OnRep_RoundIdentity()
{
	// 客户端：新局标识到达，订阅方据此丢弃旧局回调。
	OnRoundIdentityChanged.Broadcast();
}

void ABattleGameState::OnRep_MatchPhase()
{
	// 客户端：阶段到达，订阅方据此更新展示与输入门控。
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void ABattleGameState::OnRep_PhaseEndTime()
{
	// 客户端：截止时间到达，时间显示在读取属性时刷新，此处不主动推进阶段。
}

void ABattleGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABattleGameState, RoundIdentity);
	DOREPLIFETIME(ABattleGameState, MatchPhase);
	DOREPLIFETIME(ABattleGameState, PhaseEndServerTime);
	DOREPLIFETIME(ABattleGameState, TeamScores);
	DOREPLIFETIME(ABattleGameState, TeamPlayerCounts);
}
