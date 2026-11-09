// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class UInputMappingContext;
class AShooterCharacter;
class UBattleHUDWidget;
class UShooterBulletCounterUI;

/**
 * Simple PlayerController for a first person shooter game.
 * Manages input mappings, local battle HUD and template respawning.
 */
UCLASS(abstract, config="Game")
class PROJECT_FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();

	/** Update the local HUD from externally owned health state. This does not change gameplay state. */
	UFUNCTION(BlueprintCallable, Category="Shooter|Battle HUD")
	void UpdateBattleHUDHealth(float CurrentHealth, float MaxHealth);

	/** Update the local HUD from externally owned ammunition state. This does not consume ammunition. */
	UFUNCTION(BlueprintCallable, Category="Shooter|Battle HUD")
	void UpdateBattleHUDAmmo(int32 MagazineAmmo, int32 ReserveAmmo);

	/** Update the local HUD from externally owned reload state. Progress is normalized to 0-1. */
	UFUNCTION(BlueprintCallable, Category="Shooter|Battle HUD")
	void UpdateBattleHUDReload(bool bIsReloading, float NormalizedProgress);

	/** Local console command: HUDPreview 1 enables the isolated preview; HUDPreview 0 restores formal data. */
	UFUNCTION(Exec)
	void HUDPreview(bool bEnabled);

	/** Local console command: 0 auto-cycles; 1-4 select a fixed preview state. */
	UFUNCTION(Exec)
	void HUDPreviewState(int32 StateIndex);

	/** Assigns a team ID to this player. */
	void SetTeam(uint8 Team);

protected:
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category="Input|Touch Controls")
	bool bForceTouchControls = false;

	UPROPERTY(EditAnywhere, Category="Shooter|Respawn")
	TSubclassOf<AShooterCharacter> CharacterClass;

	/** Legacy template widget class retained only so existing Blueprint defaults keep loading. */
	UPROPERTY(EditAnywhere, Category="Shooter|UI", meta=(DeprecatedProperty, DeprecationMessage="Use the native Battle HUD instead."))
	TSubclassOf<UShooterBulletCounterUI> BulletCounterUIClass;

	UPROPERTY(EditAnywhere, Category="Shooter|UI")
	TSubclassOf<UBattleHUDWidget> BattleHUDClass;

	/** Local development preview. Disabled by default and never sends gameplay requests or RPCs. */
	UPROPERTY(EditAnywhere, Config, Category="Shooter|UI|Development")
	bool bEnableBattleHUDPreview = false;

	UPROPERTY(EditAnywhere, Category="Shooter|Player")
	FName PlayerPawnTag = FName("Player");

	UPROPERTY()
	TObjectPtr<UBattleHUDWidget> BattleHUD;

	uint8 TeamByte = 0;

	UPROPERTY(EditAnywhere, Category="Shooter|Team")
	TArray<FName> TeamTags;

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void OnBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	UFUNCTION()
	void OnPawnDamaged(float LifePercent);

	bool ShouldUseTouchControls() const;
	void EnsureBattleHUD();
	void UnbindFromPawn(APawn* InPawn);
};
