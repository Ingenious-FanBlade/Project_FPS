// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_FPSPlayerController.h"
#include "BattlePlayerController.generated.h"

class UBattleHUDWidget;
class UShooterBulletCounterUI;

/**
 * 正式对战玩家控制器（F08-01 的连接宿主）。
 *
 * 基类为 AProject_FPSPlayerController，避免继承模板控制器里的随机重生、
 * 队伍标记与模板 HUD 伪数据适配。
 *
 * S1 迁移 Battle HUD 生命周期（与 F09-01 原生控件共用同一控件类与 Lua 流程）：
 * 创建、更新与本地预览。不迁移 AShooterPlayerController::OnPawnDestroyed 的
 * 模板重生逻辑；重生由 F06-04 统一调度。
 */
UCLASS(config = Game)
class PROJECT_FPS_API ABattlePlayerController : public AProject_FPSPlayerController
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABattlePlayerController();

public:

	/** 更新本地 HUD 的生命显示。不改变玩法状态；权威生命由 F06 提供。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDHealth(float CurrentHealth, float MaxHealth);

	/** 更新本地 HUD 的弹药显示。不消耗弹药；权威弹药由 F05 提供。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDAmmo(int32 MagazineAmmo, int32 ReserveAmmo);

	/** 更新本地 HUD 的换弹显示，进度归一化到 0-1。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDReload(bool bIsReloading, float NormalizedProgress);

	/** 本地控制台命令：HUDPreview 1 启用隔离预览，HUDPreview 0 恢复正式数据。 */
	UFUNCTION(Exec)
	void HUDPreview(bool bEnabled);

	/** 本地控制台命令：0 自动循环，1-4 选择固定预览状态。 */
	UFUNCTION(Exec)
	void HUDPreviewState(int32 StateIndex);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	/**
	 * 输入映射上下文与触控控件由父类 AProject_FPSPlayerController 提供
	 * （DefaultMappingContexts、MobileExcludedMappingContexts、
	 * MobileControlsWidgetClass、MobileControlsWidget），本类直接复用，不重复声明。
	 */

	/** 弃用兼容字段：保留以兼容既有蓝图默认值，本类不创建旧控件。 */
	UPROPERTY(EditAnywhere, Category = "Battle HUD|UI")
	TSubclassOf<UShooterBulletCounterUI> BulletCounterUIClass;

	/** Battle HUD 控件类。 */
	UPROPERTY(EditAnywhere, Category = "Battle HUD|UI")
	TSubclassOf<UBattleHUDWidget> BattleHUDClass;

	/** 本地开发预览开关，默认关闭，不发送任何玩法请求或 RPC。 */
	UPROPERTY(EditAnywhere, Config, Category = "Battle HUD|Development")
	bool bEnableBattleHUDPreview = false;

	/** Battle HUD 实例。 */
	UPROPERTY()
	TObjectPtr<UBattleHUDWidget> BattleHUD;

	/** 确保本地玩家创建且仅创建一个 Battle HUD。 */
	void EnsureBattleHUD();
};
