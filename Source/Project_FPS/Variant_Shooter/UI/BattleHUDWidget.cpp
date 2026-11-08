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

void UBattleHUDWidget::SetDevelopmentPreviewEnabled(bool bEnabled)
{
	bDevelopmentPreviewEnabled = bEnabled;
	BP_SetDevelopmentPreviewEnabled(bEnabled);
}

void UBattleHUDWidget::SetDevelopmentPreviewState(int32 StateIndex)
{
	BP_SetDevelopmentPreviewState(FMath::Clamp(StateIndex, 0, 4));
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
}

#undef LOCTEXT_NAMESPACE
