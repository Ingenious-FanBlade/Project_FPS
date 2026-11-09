// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnLuaInterface.h"
#include "BattleHUDWidget.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;

/**
 * Native battle HUD layout for the local player.
 *
 * Gameplay data is supplied by callers. Lua owns the presentation flow and the
 * isolated development preview; this widget only builds controls and renders
 * the values it receives.
 */
UCLASS()
class PROJECT_FPS_API UBattleHUDWidget : public UUserWidget, public IUnLuaInterface
{
	GENERATED_BODY()

public:
	virtual FString GetModuleName_Implementation() const override;

	/** Pass externally owned health state into the Lua display flow. */
	void ReceiveHealthState(float CurrentHealth, float MaxHealth);

	/** Pass externally owned ammunition state into the Lua display flow. */
	void ReceiveAmmoState(int32 MagazineAmmo, int32 ReserveAmmo);

	/** Pass externally owned reload state into the Lua display flow. */
	void ReceiveReloadState(bool bIsReloading, float NormalizedProgress);

	/** Enable or disable the local-only development preview. */
	void SetDevelopmentPreviewEnabled(bool bEnabled);

	/** Select 0 for automatic cycling or 1-4 for a fixed preview state. */
	void SetDevelopmentPreviewState(int32 StateIndex);

	/** Native render endpoint used by Lua. */
	UFUNCTION(BlueprintCallable, Category="Battle HUD|Display")
	void SetHealthDisplay(float CurrentHealth, float MaxHealth);

	/** Native render endpoint used by Lua. */
	UFUNCTION(BlueprintCallable, Category="Battle HUD|Display")
	void SetAmmoDisplay(int32 MagazineAmmo, int32 ReserveAmmo);

	/** Native render endpoint used by Lua. */
	UFUNCTION(BlueprintCallable, Category="Battle HUD|Display")
	void SetReloadDisplay(bool bIsReloading, float NormalizedProgress);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_StartHUDFlow(bool bPreviewEnabled);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_StopHUDFlow();

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_TickHUDFlow(float DeltaSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_ReceiveHealthState(float CurrentHealth, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_ReceiveAmmoState(int32 MagazineAmmo, int32 ReserveAmmo);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_ReceiveReloadState(bool bIsReloading, float NormalizedProgress);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_SetDevelopmentPreviewEnabled(bool bEnabled);

	UFUNCTION(BlueprintImplementableEvent, Category="Battle HUD|Flow")
	void BP_SetDevelopmentPreviewState(int32 StateIndex);

private:
	void BuildNativeLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReloadText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ReloadBar;

	bool bDevelopmentPreviewEnabled = false;
};
