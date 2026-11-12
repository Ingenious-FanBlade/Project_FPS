// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleGameMode.h"
#include "Battle/BattleCharacter.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattlePlayerController.h"
#include "Battle/BattlePlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Project_FPS.h"

ABattleGameMode::ABattleGameMode()
{
	// 正式对战类接线。方案②以 PIE／命令行 URL 参数指定本 C++ 类，
	// 全部默认类在此显式绑定，不依赖任何蓝图资产。
	GameStateClass = ABattleGameState::StaticClass();
	PlayerStateClass = ABattlePlayerState::StaticClass();
	PlayerControllerClass = ABattlePlayerController::StaticClass();
	DefaultPawnClass = ABattleCharacter::StaticClass();
}

void ABattleGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// 注意：此处 GameState 尚未创建，RoundId 与阶段在 PostInitializeComponents 中提交。
}

void ABattleGameMode::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 此时 GameState 已创建，建立第一局标识与初始阶段。
	AdvanceRoundId();
	SetMatchPhase(EBattleMatchPhase::Waiting, 0.0);

	if (!GetBattleGameState())
	{
		UE_LOG(LogProject_FPS, Error, TEXT("ABattleGameMode: BattleGameState is not available after initialization."));
	}
}

void ABattleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// M1 直连联调：Waiting 阶段凑够配置人数后自动进入倒计时。
	// M3 起由 F03 的入场就绪与名单替代此路径。
	if (GetMatchPhase() == EBattleMatchPhase::Waiting
		&& bAllowDirectConnectAutoStart
		&& GetNumPlayers() >= DirectConnectExpectedPlayers)
	{
		SetMatchPhase(EBattleMatchPhase::Countdown, static_cast<double>(StartCountdownSeconds));

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				CountdownTimer, this, &ABattleGameMode::OnCountdownFinished, StartCountdownSeconds, false);
		}
	}
}

void ABattleGameMode::OnCountdownFinished()
{
	// 倒计时结束进入对局；M2 将在此处接入正式开局规则（分队与出生许可）。
	SetMatchPhase(EBattleMatchPhase::InProgress, 0.0);
}

bool ABattleGameMode::CanAttack(const ABattlePlayerState* InPlayerState) const
{
	// S1 只判断阶段；玩家状态由调用方（F05-03）继续叠加检查。
	(void)InPlayerState;
	return GetMatchPhase() == EBattleMatchPhase::InProgress;
}

bool ABattleGameMode::CanRespawn(const ABattlePlayerState* InPlayerState) const
{
	// S1 只判断阶段；重生计时与出生点由 F06-04 负责。
	(void)InPlayerState;
	return GetMatchPhase() == EBattleMatchPhase::InProgress;
}

bool ABattleGameMode::CanMove(const ABattlePlayerState* InPlayerState) const
{
	// Waiting 允许移动与观察；Countdown 与阶段结束类状态禁止移动。
	(void)InPlayerState;

	switch (GetMatchPhase())
	{
	case EBattleMatchPhase::Waiting:
	case EBattleMatchPhase::InProgress:
		return true;

	default:
		return false;
	}
}

FBattleRoundIdentity ABattleGameMode::GetRoundIdentity() const
{
	if (const ABattleGameState* BattleGameState = GetBattleGameState())
	{
		return BattleGameState->GetRoundIdentity();
	}

	return FBattleRoundIdentity();
}

EBattleMatchPhase ABattleGameMode::GetMatchPhase() const
{
	if (const ABattleGameState* BattleGameState = GetBattleGameState())
	{
		return BattleGameState->GetMatchPhase();
	}

	return EBattleMatchPhase::None;
}

ABattleGameState* ABattleGameMode::GetBattleGameState() const
{
	return GetGameState<ABattleGameState>();
}

void ABattleGameMode::AdvanceRoundId()
{
	ABattleGameState* BattleGameState = GetBattleGameState();
	if (!BattleGameState)
	{
		// InitGame 期间 GameState 可能尚未就绪；RoundId 在后续提交时补发。
		UE_LOG(LogProject_FPS, Warning, TEXT("ABattleGameMode::AdvanceRoundId: GameState not ready."));
		return;
	}

	FBattleRoundIdentity NewIdentity;
	NewIdentity.RoundId = NextRoundId++;
	// M1 使用本地占位 MatchId；M3 起由大厅／匹配服务注入。
	NewIdentity.MatchId = FGuid::NewGuid();
	NewIdentity.ServerStartTime = GetServerTimeSeconds();

	BattleGameState->SetRoundIdentity(NewIdentity);
}

void ABattleGameMode::SetMatchPhase(EBattleMatchPhase NewPhase, double PhaseDurationSeconds)
{
	ABattleGameState* BattleGameState = GetBattleGameState();
	if (!BattleGameState)
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("ABattleGameMode::SetMatchPhase: GameState not ready."));
		return;
	}

	const double EndTime = (PhaseDurationSeconds > 0.0)
		? GetServerTimeSeconds() + PhaseDurationSeconds
		: 0.0;

	BattleGameState->SetMatchPhase(NewPhase, EndTime);
}

double ABattleGameMode::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
