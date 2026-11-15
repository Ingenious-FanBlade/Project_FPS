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
	ResetBattleHUDCombatFeedback();

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
	ResetBattleHUDCombatFeedback();
	UpdateBattleHUDAmmo(0, 0);
	UpdateBattleHUDReload(false, 0.0f);
	Super::OnUnPossess();
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	UnbindFromPawn(Cast<APawn>(DestroyedActor));
	ResetBattleHUDCombatFeedback();
	UpdateBattleHUDAmmo(0, 0);
	UpdateBattleHUDReload(false, 0.0f);
}

void AShooterPlayerController::RespawnTemplateCharacter(TSubclassOf<AShooterCharacter> FallbackClass)
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	UClass* SpawnClass = CharacterClass ? CharacterClass.Get() : FallbackClass.Get();
	if (!SpawnClass)
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("Template respawn failed: no character class is configured."));
		return;
	}

	TArray<AActor*> ActorList;
	if (TeamTags.IsValidIndex(TeamByte))
	{
		UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), TeamTags[TeamByte], ActorList);
	}
	if (ActorList.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);
	}
	if (ActorList.IsEmpty())
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("Template respawn failed: no PlayerStart exists in the current map."));
		return;
	}

	const AActor* PlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(SpawnClass, PlayerStart->GetActorTransform(), SpawnParameters))
	{
		Possess(RespawnedCharacter);
		UE_LOG(LogProject_FPS, Log, TEXT("Template respawn possessed %s at %s."), *GetNameSafe(RespawnedCharacter), *GetNameSafe(PlayerStart));
	}
	else
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("Template respawn failed: could not spawn %s."), *GetNameSafe(SpawnClass));
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

void AShooterPlayerController::UpdateBattleHUDHitConfirmed()
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveHitConfirmed();
	}
}

void AShooterPlayerController::UpdateBattleHUDDamageFeedback()
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveDamageFeedback();
	}
}

void AShooterPlayerController::UpdateBattleHUDDeath(bool bIsDead, float RemainingSeconds)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveDeathState(bIsDead, RemainingSeconds);
	}
}

void AShooterPlayerController::UpdateBattleHUDProtection(bool bProtected, float RemainingSeconds)
{
	EnsureBattleHUD();
	if (IsValid(BattleHUD))
	{
		BattleHUD->ReceiveProtectionState(bProtected, RemainingSeconds);
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

void AShooterPlayerController::ResetBattleHUDCombatFeedback()
{
	if (IsValid(BattleHUD))
	{
		BattleHUD->ResetCombatFeedback();
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
