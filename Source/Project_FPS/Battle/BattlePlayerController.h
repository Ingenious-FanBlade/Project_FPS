// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_FPSPlayerController.h"
#include "BattlePlayerController.generated.h"

class UBattleHUDWidget;
class UBattleWeaponComponent;
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

	/** 仅在外部权威流程确认有效命中后显示短暂命中标记。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDHitConfirmed();

	/** 仅在外部权威流程确认受伤后显示短暂受伤反馈。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDDamageFeedback();

	/** 显示外部根据服务器截止时间计算的非负重生剩余秒数。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDDeath(bool bIsDead, float RemainingSeconds);

	/** 显示外部根据服务器截止时间计算的非负保护剩余秒数。 */
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void UpdateBattleHUDProtection(bool bProtected, float RemainingSeconds);

	/** 本地控制台命令：HUDPreview 1 启用隔离预览，HUDPreview 0 恢复正式数据。 */
	UFUNCTION(Exec)
	void HUDPreview(bool bEnabled);

	/** 本地控制台命令：0 自动循环，1-9 选择固定预览状态。 */
	UFUNCTION(Exec)
	void HUDPreviewState(int32 StateIndex);

	/**
	 * 本地调试命令：模拟一次开火请求，走完整 Server_Fire 校验链。
	 *
	 * Exec 放在 PlayerController 上才能在窗口控制台被直接接收（Pawn 不是命令接收者）。
	 * 实现仅在非 Shipping 构建中提供（见 BattlePlayerController.cpp）。
	 */
	UFUNCTION(Exec)
	void BattleTestFire(int32 Count = 1);

	/** 本地调试命令：模拟一次换弹请求，走完整 Server_Reload 校验链。 */
	UFUNCTION(Exec)
	void BattleTestReload();

	/** 本地调试命令：向前瞬移指定距离，用于验证服务端位移校验与拉回。 */
	UFUNCTION(Exec)
	void BattleTestTeleport(float Distance = 1000.0f);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_Pawn() override;

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

	/** 将正式 Battle Pawn 的拥有者可见武器状态接入 HUD。 */
	void RefreshPawnBinding();

	/** 解除旧 Pawn 与武器组件绑定并停止换弹进度刷新。 */
	void UnbindFromBattlePawn();

	/** 从武器组件读取同一帧完整快照并刷新弹药与换弹显示。 */
	void HandleWeaponStateChanged();

	/** 根据同步服务器时间刷新换弹进度。 */
	void RefreshReloadHUD();

	/** 清理 F09-03 的本地短暂表现、死亡和保护状态。 */
	void ResetBattleHUDCombatFeedback();

	/** 旧 Pawn 销毁时立即清理绑定，避免等待复制状态更新。 */
	UFUNCTION()
	void HandleBoundPawnDestroyed(AActor* DestroyedActor);

	TWeakObjectPtr<APawn> BoundBattlePawn;
	TWeakObjectPtr<UBattleWeaponComponent> BoundWeaponComponent;
	FTimerHandle ReloadHUDTimer;
};
