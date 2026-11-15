// Copyright Epic Games, Inc. All Rights Reserved.

#include "BattleHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "BattleHUDWidget"

namespace
{
	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FName Name, const FText& Text, int32 FontSize)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UBorder* MakePanel(UWidgetTree* WidgetTree, const FName Name)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.82f));
		Panel->SetPadding(FMargin(16.0f, 12.0f));
		return Panel;
	}
}

FString UBattleHUDWidget::GetModuleName_Implementation() const
{
	return TEXT("Client.UI.BattleHUD");
}

void UBattleHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildNativeLayout();
	BP_StartHUDFlow(bDevelopmentPreviewEnabled);
}

void UBattleHUDWidget::NativeDestruct()
{
	BP_StopHUDFlow();
	Super::NativeDestruct();
}

void UBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BP_TickHUDFlow(InDeltaTime);
}

void UBattleHUDWidget::ReceiveHealthState(float CurrentHealth, float MaxHealth)
{
	BP_ReceiveHealthState(CurrentHealth, MaxHealth);
}

void UBattleHUDWidget::ReceiveAmmoState(int32 MagazineAmmo, int32 ReserveAmmo)
{
	BP_ReceiveAmmoState(MagazineAmmo, ReserveAmmo);
}

void UBattleHUDWidget::ReceiveReloadState(bool bIsReloading, float NormalizedProgress)
{
	BP_ReceiveReloadState(bIsReloading, NormalizedProgress);
}

void UBattleHUDWidget::ReceiveHitConfirmed()
{
	BP_ReceiveHitConfirmed();
}

void UBattleHUDWidget::ReceiveDamageFeedback()
{
	BP_ReceiveDamageFeedback();
}

void UBattleHUDWidget::ReceiveDeathState(bool bIsDead, float RemainingSeconds)
{
	BP_ReceiveDeathState(bIsDead, RemainingSeconds);
}

void UBattleHUDWidget::ReceiveProtectionState(bool bProtected, float RemainingSeconds)
{
	BP_ReceiveProtectionState(bProtected, RemainingSeconds);
}

void UBattleHUDWidget::ResetCombatFeedback()
{
	BP_ResetCombatFeedback();
}

void UBattleHUDWidget::SetDevelopmentPreviewEnabled(bool bEnabled)
{
	bDevelopmentPreviewEnabled = bEnabled;
	BP_SetDevelopmentPreviewEnabled(bEnabled);
}

void UBattleHUDWidget::SetDevelopmentPreviewState(int32 StateIndex)
{
	BP_SetDevelopmentPreviewState(FMath::Clamp(StateIndex, 0, 9));
}

void UBattleHUDWidget::SetHealthDisplay(float CurrentHealth, float MaxHealth)
{
	if (!HealthText || !HealthBar)
	{
		return;
	}

	const float SafeMaxHealth = FMath::Max(0.0f, MaxHealth);
	const float SafeCurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, SafeMaxHealth);
	const float HealthPercent = SafeMaxHealth > 0.0f ? SafeCurrentHealth / SafeMaxHealth : 0.0f;

	HealthText->SetText(FText::Format(
		LOCTEXT("HealthFormat", "生命  {0} / {1}"),
		FText::AsNumber(FMath::RoundToInt(SafeCurrentHealth)),
		FText::AsNumber(FMath::RoundToInt(SafeMaxHealth))));
	HealthBar->SetPercent(HealthPercent);
	HealthBar->SetFillColorAndOpacity(HealthPercent > 0.3f
		? FLinearColor(0.15f, 0.78f, 0.35f, 1.0f)
		: FLinearColor(0.92f, 0.18f, 0.12f, 1.0f));
}

void UBattleHUDWidget::SetAmmoDisplay(int32 MagazineAmmo, int32 ReserveAmmo)
{
	if (!AmmoText)
	{
		return;
	}

	AmmoText->SetText(FText::Format(
		LOCTEXT("AmmoFormat", "弹药  {0} / {1}"),
		FText::AsNumber(FMath::Max(0, MagazineAmmo)),
		FText::AsNumber(FMath::Max(0, ReserveAmmo))));
}

void UBattleHUDWidget::SetReloadDisplay(bool bIsReloading, float NormalizedProgress)
{
	if (!ReloadText || !ReloadBar)
	{
		return;
	}

	const ESlateVisibility ReloadVisibility = bIsReloading
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	ReloadText->SetVisibility(ReloadVisibility);
	ReloadBar->SetVisibility(ReloadVisibility);

	const float SafeProgress = FMath::Clamp(NormalizedProgress, 0.0f, 1.0f);
	ReloadBar->SetPercent(SafeProgress);
	ReloadText->SetText(FText::Format(
		LOCTEXT("ReloadFormat", "换弹中  {0}%"),
		FText::AsNumber(FMath::RoundToInt(SafeProgress * 100.0f))));
}

void UBattleHUDWidget::SetHitDisplay(bool bVisible)
{
	if (HitText)
	{
		HitText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UBattleHUDWidget::SetDamageDisplay(float Opacity)
{
	if (DamageOverlay)
	{
		const float SafeOpacity = FMath::Clamp(Opacity, 0.0f, 0.35f);
		DamageOverlay->SetBrushColor(FLinearColor(0.8f, 0.025f, 0.025f, SafeOpacity));
		DamageOverlay->SetVisibility(SafeOpacity > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UBattleHUDWidget::SetDeathDisplay(bool bIsDead, float RemainingSeconds)
{
	if (DeathPanel && DeathText)
	{
		DeathPanel->SetVisibility(bIsDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DeathText->SetText(FText::Format(
			LOCTEXT("RespawnCountdown", "等待重生  {0} 秒"),
			FText::AsNumber(FMath::CeilToInt(FMath::Max(0.0f, RemainingSeconds)))));
	}
}

void UBattleHUDWidget::SetProtectionDisplay(bool bProtected, float RemainingSeconds)
{
	if (ProtectionPanel && ProtectionText)
	{
		ProtectionPanel->SetVisibility(bProtected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ProtectionText->SetText(FText::Format(
			LOCTEXT("ProtectionCountdown", "重生保护  {0} 秒"),
			FText::AsNumber(FMath::Max(0.0f, RemainingSeconds))));
	}
}

void UBattleHUDWidget::BuildNativeLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BattleHUDRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	// Crosshair: two simple native bars, centered independently of viewport size.
	UBorder* CrosshairVertical = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrosshairVertical"));
	CrosshairVertical->SetBrushColor(FLinearColor::White);
	UCanvasPanelSlot* VerticalSlot = RootCanvas->AddChildToCanvas(CrosshairVertical);
	VerticalSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	VerticalSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	VerticalSlot->SetPosition(FVector2D::ZeroVector);
	VerticalSlot->SetSize(FVector2D(2.0f, 18.0f));

	UBorder* CrosshairHorizontal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CrosshairHorizontal"));
	CrosshairHorizontal->SetBrushColor(FLinearColor::White);
	UCanvasPanelSlot* HorizontalSlot = RootCanvas->AddChildToCanvas(CrosshairHorizontal);
	HorizontalSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	HorizontalSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	HorizontalSlot->SetPosition(FVector2D::ZeroVector);
	HorizontalSlot->SetSize(FVector2D(18.0f, 2.0f));

	// Bottom-left health panel.
	UBorder* HealthPanel = MakePanel(WidgetTree, TEXT("HealthPanel"));
	UCanvasPanelSlot* HealthPanelSlot = RootCanvas->AddChildToCanvas(HealthPanel);
	HealthPanelSlot->SetAnchors(FAnchors(0.0f, 1.0f));
	HealthPanelSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	HealthPanelSlot->SetPosition(FVector2D(32.0f, -32.0f));
	HealthPanelSlot->SetSize(FVector2D(320.0f, 88.0f));

	UVerticalBox* HealthColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HealthColumn"));
	HealthPanel->SetContent(HealthColumn);
	HealthText = MakeText(WidgetTree, TEXT("HealthText"), LOCTEXT("HealthUnavailable", "生命  -- / --"), 20);
	HealthColumn->AddChildToVerticalBox(HealthText);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetPercent(0.0f);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.78f, 0.35f, 1.0f));
	UVerticalBoxSlot* HealthBarSlot = HealthColumn->AddChildToVerticalBox(HealthBar);
	HealthBarSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

	// Bottom-right ammunition and reload panel.
	UBorder* AmmoPanel = MakePanel(WidgetTree, TEXT("AmmoPanel"));
	UCanvasPanelSlot* AmmoPanelSlot = RootCanvas->AddChildToCanvas(AmmoPanel);
	AmmoPanelSlot->SetAnchors(FAnchors(1.0f, 1.0f));
	AmmoPanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	AmmoPanelSlot->SetPosition(FVector2D(-32.0f, -32.0f));
	AmmoPanelSlot->SetSize(FVector2D(320.0f, 116.0f));

	UVerticalBox* AmmoColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AmmoColumn"));
	AmmoPanel->SetContent(AmmoColumn);
	AmmoText = MakeText(WidgetTree, TEXT("AmmoText"), LOCTEXT("AmmoUnavailable", "弹药  -- / --"), 24);
	AmmoText->SetJustification(ETextJustify::Right);
	AmmoColumn->AddChildToVerticalBox(AmmoText);
	ReloadText = MakeText(WidgetTree, TEXT("ReloadText"), LOCTEXT("ReloadInitial", "换弹中  0%"), 16);
	ReloadText->SetJustification(ETextJustify::Right);
	UVerticalBoxSlot* ReloadTextSlot = AmmoColumn->AddChildToVerticalBox(ReloadText);
	ReloadTextSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	ReloadBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ReloadBar"));
	ReloadBar->SetPercent(0.0f);
	ReloadBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.72f, 0.18f, 1.0f));
	AmmoColumn->AddChildToVerticalBox(ReloadBar);
	SetReloadDisplay(false, 0.0f);

	// Full-screen tint and short-lived hit marker. These never intercept input.
	DamageOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DamageOverlay"));
	UCanvasPanelSlot* DamageSlot = RootCanvas->AddChildToCanvas(DamageOverlay);
	DamageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DamageSlot->SetOffsets(FMargin(0.0f));
	SetDamageDisplay(0.0f);

	HitText = MakeText(WidgetTree, TEXT("HitText"), LOCTEXT("HitConfirmed", "命中"), 20);
	HitText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* HitSlot = RootCanvas->AddChildToCanvas(HitText);
	HitSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	HitSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	HitSlot->SetPosition(FVector2D(0.0f, 25.0f));
	HitSlot->SetSize(FVector2D(100.0f, 30.0f));
	SetHitDisplay(false);

	DeathPanel = MakePanel(WidgetTree, TEXT("DeathPanel"));
	UCanvasPanelSlot* DeathSlot = RootCanvas->AddChildToCanvas(DeathPanel);
	DeathSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	DeathSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	DeathSlot->SetPosition(FVector2D(0.0f, 85.0f));
	DeathSlot->SetSize(FVector2D(280.0f, 65.0f));
	DeathText = MakeText(WidgetTree, TEXT("DeathText"), FText::GetEmpty(), 22);
	DeathText->SetJustification(ETextJustify::Center);
	DeathPanel->SetContent(DeathText);
	SetDeathDisplay(false, 0.0f);

	ProtectionPanel = MakePanel(WidgetTree, TEXT("ProtectionPanel"));
	UCanvasPanelSlot* ProtectionSlot = RootCanvas->AddChildToCanvas(ProtectionPanel);
	ProtectionSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	ProtectionSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	ProtectionSlot->SetPosition(FVector2D(0.0f, 104.0f));
	ProtectionSlot->SetSize(FVector2D(260.0f, 55.0f));
	ProtectionText = MakeText(WidgetTree, TEXT("ProtectionText"), FText::GetEmpty(), 18);
	ProtectionText->SetJustification(ETextJustify::Center);
	ProtectionPanel->SetContent(ProtectionText);
	SetProtectionDisplay(false, 0.0f);
}

#undef LOCTEXT_NAMESPACE
