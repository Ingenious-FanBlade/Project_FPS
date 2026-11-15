// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattlePlayerController.h"

#include "Battle/BattleCharacter.h"
#include "BattleHUDWidget.h"
#include "InputMappingContext.h"
#include "Project_FPS.h"
#include "ShooterBulletCounterUI.h"
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

	// 不在此处写入生命值：模板角色的比例式生命是伪数据，权威生命由 F06 提供。
}

void ABattlePlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
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
