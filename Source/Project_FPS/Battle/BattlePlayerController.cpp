// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattlePlayerController.h"

#include "BattleHUDWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
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
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}

		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

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
