// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Shooter/ShooterPlayerController.h"

#include "BattleHUDWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerStart.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Project_FPS.h"
#include "ShooterCharacter.h"
#include "Widgets/Input/SVirtualJoystick.h"

AShooterPlayerController::AShooterPlayerController()
{
	BattleHUDClass = UBattleHUDWidget::StaticClass();
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->SetDevelopmentPreviewEnabled(bEnableBattleHUDPreview);
	}

	// The template currently exposes health as a normalized value only. This
	// adapter is replaced by real current/max values through the public API.
	OnPawnDamaged(1.0f);
}

void AShooterPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPawn(GetPawn());

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

void AShooterPlayerController::SetupInputComponent()
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

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	InPawn->OnDestroyed.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDestroyed);
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		ShooterCharacter->Tags.AddUnique(PlayerPawnTag);
		ShooterCharacter->SetTeam(TeamByte);
		ShooterCharacter->OnBulletCountUpdated.AddUniqueDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.AddUniqueDynamic(this, &AShooterPlayerController::OnPawnDamaged);

		ShooterCharacter->OnDamaged.Broadcast(1.0f);
	}
}

void AShooterPlayerController::OnUnPossess()
{
	UnbindFromPawn(GetPawn());
	Super::OnUnPossess();
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	UnbindFromPawn(Cast<APawn>(DestroyedActor));
	UpdateBattleHUDAmmo(0, 0);
	UpdateBattleHUDReload(false, 0.0f);

	if (TeamByte < TeamTags.Num())
	{
		TArray<AActor*> ActorList;
		UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), TeamTags[TeamByte], ActorList);

		if (ActorList.Num() > 0)
		{
			AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];
			const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

			if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
			{
				Possess(RespawnedCharacter);
			}
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets)
{
	(void)MagazineSize;

	// The template contract has current rounds and magazine capacity, but no
	// reserve-ammo value. Formal combat state must supply both values via the API.
	UpdateBattleHUDAmmo(Bullets, 0);
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	// The template delegate reports only a ratio. Present it against the design
	// baseline of 100 until current/max health replication is available.
	UpdateBattleHUDHealth(FMath::Clamp(LifePercent, 0.0f, 1.0f) * 100.0f, 100.0f);
}

void AShooterPlayerController::UpdateBattleHUDHealth(float CurrentHealth, float MaxHealth)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveHealthState(CurrentHealth, MaxHealth);
	}
}

void AShooterPlayerController::UpdateBattleHUDAmmo(int32 MagazineAmmo, int32 ReserveAmmo)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveAmmoState(MagazineAmmo, ReserveAmmo);
	}
}

void AShooterPlayerController::UpdateBattleHUDReload(bool bIsReloading, float NormalizedProgress)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveReloadState(bIsReloading, NormalizedProgress);
	}
}

void AShooterPlayerController::HUDPreview(bool bEnabled)
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

void AShooterPlayerController::HUDPreviewState(int32 StateIndex)
{
	if (IsLocalPlayerController() && IsValid(BattleHUD))
	{
		BattleHUD->SetDevelopmentPreviewState(StateIndex);
	}
}

void AShooterPlayerController::EnsureBattleHUD()
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

void AShooterPlayerController::UnbindFromPawn(APawn* InPawn)
{
	if (!IsValid(InPawn))
	{
		return;
	}

	InPawn->OnDestroyed.RemoveDynamic(this, &AShooterPlayerController::OnPawnDestroyed);
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		ShooterCharacter->OnBulletCountUpdated.RemoveDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.RemoveDynamic(this, &AShooterPlayerController::OnPawnDamaged);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AShooterPlayerController::SetTeam(uint8 Team)
{
	TeamByte = Team;

	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterCharacter->SetTeam(Team);
	}
}
