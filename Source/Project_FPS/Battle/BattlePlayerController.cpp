// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattlePlayerController.h"

#include "Battle/BattleCharacter.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattleTypes.h"
#include "Battle/Components/BattleWeaponComponent.h"
#include "BattleHUDWidget.h"
#include "GameFramework/GameStateBase.h"
#include "InputMappingContext.h"
#include "Project_FPS.h"
#include "ShooterBulletCounterUI.h"
#include "TimerManager.h"
#include "Widgets/Input/SVirtualJoystick.h"

ABattlePlayerController::ABattlePlayerController()
{
	BattleHUDClass = UBattleHUDWidget::StaticClass();
}

void ABattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->SetDevelopmentPreviewEnabled(bEnableBattleHUDPreview);
	}
	RefreshPawnBinding();

	// 不在此处写入生命值：模板角色的比例式生命是伪数据，权威生命由 F06 提供。
}

void ABattlePlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	UnbindFromBattlePawn();
	ResetBattleHUDCombatFeedback();

	if (IsValid(BattleHUD))
	{
		BattleHUD->RemoveFromParent();
		BattleHUD = nullptr;
	}

	if (IsValid(MobileControlsWidget))
	{
		MobileControlsWidget->RemoveFromParent();
		MobileControlsWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ABattlePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshPawnBinding();
}

void ABattlePlayerController::OnUnPossess()
{
	UnbindFromBattlePawn();
	ResetBattleHUDCombatFeedback();
	UpdateBattleHUDAmmo(0, 0);
	UpdateBattleHUDReload(false, 0.0f);
	Super::OnUnPossess();
}

void ABattlePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	RefreshPawnBinding();
}

void ABattlePlayerController::SetupInputComponent()
{
	// 方案②以 C++ 类直接启动，没有蓝图实例配置映射上下文；
	// 若 DefaultMappingContexts 为空，Subsystem 不会添加任何 IMC，输入动作无法触发。
	if (DefaultMappingContexts.Num() == 0)
	{
		if (UInputMappingContext* DefaultContext = LoadObject<UInputMappingContext>(
				nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			DefaultMappingContexts.Add(DefaultContext);
		}
		else
		{
			UE_LOG(LogProject_FPS, Error, TEXT("%s: failed to load IMC_Default."), *GetName());
		}
	}

	if (MobileExcludedMappingContexts.Num() == 0)
	{
		if (UInputMappingContext* MouseLookContext = LoadObject<UInputMappingContext>(
				nullptr, TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")))
		{
			MobileExcludedMappingContexts.Add(MouseLookContext);
		}
		else
		{
			UE_LOG(LogProject_FPS, Error, TEXT("%s: failed to load IMC_MouseLook."), *GetName());
		}
	}

	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	// 映射上下文的 AddMappingContext 已由基类 SetupInputComponent 完成；
	// 子类只负责在此前填好数组，不重复添加同一 IMC。

	if (ShouldUseTouchControls() && !IsValid(MobileControlsWidget))
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogProject_FPS, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}

	EnsureBattleHUD();
}

void ABattlePlayerController::UpdateBattleHUDHealth(float CurrentHealth, float MaxHealth)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveHealthState(CurrentHealth, MaxHealth);
	}
}

void ABattlePlayerController::UpdateBattleHUDAmmo(int32 MagazineAmmo, int32 ReserveAmmo)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveAmmoState(MagazineAmmo, ReserveAmmo);
	}
}

void ABattlePlayerController::UpdateBattleHUDReload(bool bIsReloading, float NormalizedProgress)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveReloadState(bIsReloading, NormalizedProgress);
	}
}

void ABattlePlayerController::UpdateBattleHUDHitConfirmed()
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveHitConfirmed();
	}
}

void ABattlePlayerController::UpdateBattleHUDDamageFeedback()
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveDamageFeedback();
	}
}

void ABattlePlayerController::UpdateBattleHUDDeath(bool bIsDead, float RemainingSeconds)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveDeathState(bIsDead, RemainingSeconds);
	}
}

void ABattlePlayerController::UpdateBattleHUDProtection(bool bProtected, float RemainingSeconds)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveProtectionState(bProtected, RemainingSeconds);
	}
}

void ABattlePlayerController::HUDPreview(bool bEnabled)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	bEnableBattleHUDPreview = bEnabled;
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->SetDevelopmentPreviewEnabled(bEnabled);
	}
}

void ABattlePlayerController::HUDPreviewState(int32 StateIndex)
{
	if (IsLocalPlayerController() && IsValid(BattleHUD))
	{
		BattleHUD->SetDevelopmentPreviewState(StateIndex);
	}
}

void ABattlePlayerController::BattleTestFire(int32 Count)
{
	UE_LOG(LogTemp, Warning, TEXT("TestFire: PC Called. Count=%d LocalController=%s"),
		Count, IsLocalController() ? TEXT("yes") : TEXT("no"));

	// 只有本地客户端窗口的命令才有意义；服务器窗口不发送开火请求。
	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestFire: not a local controller, aborting."));
		return;
	}

	ABattleCharacter* BattleCharacter = GetPawn<ABattleCharacter>();
	if (!BattleCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestFire: no ABattleCharacter possessed, aborting."));
		return;
	}

	const int32 SafeCount = FMath::Clamp(Count, 1, 32);
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		BattleCharacter->DebugTriggerFire();
	}
}

void ABattlePlayerController::BattleTestReload()
{
	UE_LOG(LogTemp, Warning, TEXT("TestReload: PC Called. LocalController=%s"),
		IsLocalController() ? TEXT("yes") : TEXT("no"));

	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestReload: not a local controller, aborting."));
		return;
	}

	ABattleCharacter* BattleCharacter = GetPawn<ABattleCharacter>();
	if (!BattleCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestReload: no ABattleCharacter possessed, aborting."));
		return;
	}

	BattleCharacter->DebugTriggerReload();
}

void ABattlePlayerController::BattleTestTeleport(float Distance)
{
	UE_LOG(LogTemp, Warning, TEXT("TestTeleport: PC Called. LocalController=%s Distance=%.0f"),
		IsLocalController() ? TEXT("yes") : TEXT("no"), Distance);

	if (!IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("TestTeleport: not a local controller, aborting."));
		return;
	}

	ABattleCharacter* BattleCharacter = GetPawn<ABattleCharacter>();
	if (!BattleCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestTeleport: no ABattleCharacter possessed, aborting."));
		return;
	}

	BattleCharacter->DebugTeleport(Distance);
}

void ABattlePlayerController::BattleTestPhase(int32 PhaseValue)
{
	UE_LOG(LogTemp, Warning, TEXT("TestPhase: PC Called. PhaseValue=%d Authority=%s LocalController=%s"),
		PhaseValue, HasAuthority() ? TEXT("yes") : TEXT("no"),
		IsLocalController() ? TEXT("yes") : TEXT("no"));

	const UEnum* PhaseEnum = StaticEnum<EBattleMatchPhase>();
	if (!PhaseEnum || !PhaseEnum->IsValidEnumValue(PhaseValue))
	{
		UE_LOG(LogTemp, Warning, TEXT("TestPhase: invalid phase value %d (expected 0-6)."), PhaseValue);
		return;
	}

	// 服务器窗口直接执行；客户端窗口经 Server RPC 转发到服务器执行。
	if (HasAuthority())
	{
		Server_BattleTestPhase_Implementation(PhaseValue);
	}
	else if (IsLocalController())
	{
		Server_BattleTestPhase(PhaseValue);
		UE_LOG(LogTemp, Warning, TEXT("TestPhase: forwarded to server via RPC."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TestPhase: not a local controller, aborting."));
	}
}

void ABattlePlayerController::Server_BattleTestPhase_Implementation(int32 PhaseValue)
{
	// 只有服务器可以提交阶段。
	if (!HasAuthority())
	{
		return;
	}

	const UEnum* PhaseEnum = StaticEnum<EBattleMatchPhase>();
	if (!PhaseEnum || !PhaseEnum->IsValidEnumValue(PhaseValue))
	{
		return;
	}

	ABattleGameState* BattleGameState = GetWorld() ? GetWorld()->GetGameState<ABattleGameState>() : nullptr;
	if (!BattleGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("TestPhase: ABattleGameState not available."));
		return;
	}

	const EBattleMatchPhase NewPhase = static_cast<EBattleMatchPhase>(PhaseValue);
	BattleGameState->SetMatchPhase(NewPhase, 0.0);

	UE_LOG(LogTemp, Warning, TEXT("TestPhase: server set phase -> %s."),
		*PhaseEnum->GetNameStringByValue(PhaseValue));
}

void ABattlePlayerController::EnsureBattleHUD()
{
	if (!IsLocalPlayerController() || GetNetMode() == NM_DedicatedServer || IsValid(BattleHUD) || !BattleHUDClass)
	{
		return;
	}

	BattleHUD = CreateWidget<UBattleHUDWidget>(this, BattleHUDClass);
	if (BattleHUD)
	{
		BattleHUD->AddToPlayerScreen(10);
	}
	else
	{
		UE_LOG(LogProject_FPS, Error, TEXT("Could not spawn the native battle HUD widget."));
	}
}

void ABattlePlayerController::RefreshPawnBinding()
{
	if (!IsLocalPlayerController())
	{
		UnbindFromBattlePawn();
		return;
	}

	APawn* CurrentPawn = GetPawn();
	ABattleCharacter* BattleCharacter = Cast<ABattleCharacter>(CurrentPawn);
	UBattleWeaponComponent* WeaponComponent = BattleCharacter ? BattleCharacter->GetWeaponComponent() : nullptr;
	if (BoundBattlePawn.Get() == CurrentPawn && BoundWeaponComponent.Get() == WeaponComponent)
	{
		HandleWeaponStateChanged();
		return;
	}

	UnbindFromBattlePawn();
	ResetBattleHUDCombatFeedback();

	if (!IsValid(CurrentPawn) || !IsValid(WeaponComponent))
	{
		UpdateBattleHUDAmmo(0, 0);
		UpdateBattleHUDReload(false, 0.0f);
		return;
	}

	BoundBattlePawn = CurrentPawn;
	BoundWeaponComponent = WeaponComponent;
	CurrentPawn->OnDestroyed.AddUniqueDynamic(this, &ABattlePlayerController::HandleBoundPawnDestroyed);
	WeaponComponent->OnWeaponStateChanged.AddUObject(this, &ABattlePlayerController::HandleWeaponStateChanged);

	// RepNotify 可能早于 Controller 完成绑定，因此绑定后必须主动读取一次完整快照。
	HandleWeaponStateChanged();
}

void ABattlePlayerController::UnbindFromBattlePawn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadHUDTimer);
	}

	if (UBattleWeaponComponent* WeaponComponent = BoundWeaponComponent.Get())
	{
		WeaponComponent->OnWeaponStateChanged.RemoveAll(this);
	}
	if (APawn* CurrentBoundPawn = BoundBattlePawn.Get())
	{
		CurrentBoundPawn->OnDestroyed.RemoveDynamic(this, &ABattlePlayerController::HandleBoundPawnDestroyed);
	}

	BoundWeaponComponent.Reset();
	BoundBattlePawn.Reset();
}

void ABattlePlayerController::HandleWeaponStateChanged()
{
	UBattleWeaponComponent* WeaponComponent = BoundWeaponComponent.Get();
	if (!IsValid(WeaponComponent))
	{
		UnbindFromBattlePawn();
		UpdateBattleHUDAmmo(0, 0);
		UpdateBattleHUDReload(false, 0.0f);
		return;
	}

	UpdateBattleHUDAmmo(WeaponComponent->GetMagazineAmmo(), WeaponComponent->GetReserveAmmo());

	UWorld* World = GetWorld();
	if (!WeaponComponent->IsReloading() || !World)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(ReloadHUDTimer);
		}
		UpdateBattleHUDReload(false, 0.0f);
		return;
	}

	RefreshReloadHUD();
	if (!World->GetTimerManager().IsTimerActive(ReloadHUDTimer))
	{
		World->GetTimerManager().SetTimer(
			ReloadHUDTimer, this, &ABattlePlayerController::RefreshReloadHUD, 0.05f, true);
	}
}

void ABattlePlayerController::RefreshReloadHUD()
{
	UBattleWeaponComponent* WeaponComponent = BoundWeaponComponent.Get();
	ABattleCharacter* BattleCharacter = Cast<ABattleCharacter>(BoundBattlePawn.Get());
	UWorld* World = GetWorld();
	if (!IsValid(WeaponComponent) || !IsValid(BattleCharacter) || !World || !WeaponComponent->IsReloading())
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(ReloadHUDTimer);
		}
		UpdateBattleHUDReload(false, 0.0f);
		return;
	}

	const float ReloadDuration = BattleCharacter->GetWeaponReloadDuration();
	const AGameStateBase* GameState = World->GetGameState();
	if (ReloadDuration <= 0.0f || !GameState)
	{
		UpdateBattleHUDReload(true, 0.0f);
		return;
	}

	const double ServerNow = static_cast<double>(GameState->GetServerWorldTimeSeconds());
	const double Elapsed = FMath::Max(0.0, ServerNow - WeaponComponent->GetReloadStartServerTime());
	const float Progress = FMath::Clamp(static_cast<float>(Elapsed / ReloadDuration), 0.0f, 1.0f);
	UpdateBattleHUDReload(true, Progress);
}

void ABattlePlayerController::ResetBattleHUDCombatFeedback()
{
	if (IsValid(BattleHUD))
	{
		BattleHUD->ResetCombatFeedback();
	}
}

void ABattlePlayerController::HandleBoundPawnDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != BoundBattlePawn.Get())
	{
		return;
	}

	UnbindFromBattlePawn();
	ResetBattleHUDCombatFeedback();
	UpdateBattleHUDAmmo(0, 0);
	UpdateBattleHUDReload(false, 0.0f);
}
