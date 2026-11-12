// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattlePlayerState.h"
#include "Net/UnrealNetwork.h"

ABattlePlayerState::ABattlePlayerState()
{
	// 玩家公共状态需要复制到所有客户端（记分板与队伍识别）。
	bReplicates = true;
}

void ABattlePlayerState::SetTeam(EBattleTeamId InTeamId)
{
	// 只有服务器可以提交队伍。
	if (!HasAuthority())
	{
		return;
	}

	TeamId = InTeamId;
}

void ABattlePlayerState::OnRep_TeamId()
{
	// 客户端：队伍到达。当前没有内建订阅方，S3／F09 读取 GetTeam() 刷新识别表现。
}

void ABattlePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABattlePlayerState, TeamId);
	DOREPLIFETIME(ABattlePlayerState, Kills);
	DOREPLIFETIME(ABattlePlayerState, Deaths);
}
