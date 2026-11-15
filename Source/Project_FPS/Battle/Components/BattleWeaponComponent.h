// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BattleWeaponComponent.generated.h"

class AShooterWeapon;

/**
 * 战斗角色武器组件（F05-01）。
 *
 * 职责：持有当前武器 Actor 引用与权威弹药/换弹状态，并为 F05-03/04/05 提供
 * 读取入口。本组件不实现开火、射线、伤害与换弹逻辑。
 *
 * 权威归属：弹药与换弹状态由服务器写入；客户端只读取（弹药为拥有者可见）。
 * 注意：AShooterWeapon 自身的 CurrentBullets 不复制且无 setter，不具权威性，
 * 本组件的 MagazineAmmo 才是唯一权威来源。
 */
UCLASS(ClassGroup = (Battle), meta = (BlueprintSpawnableComponent))
class PROJECT_FPS_API UBattleWeaponComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UBattleWeaponComponent();

public:

	//~Begin F05-01 已实现：装备引用与出生弹药初始化

	/** 设置当前武器。执行端：两端（服务器生成后复制；客户端在挂接时设置）。 */
	void SetCurrentWeapon(AShooterWeapon* InWeapon);

	/** 返回当前武器，无武器时为 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	AShooterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	/**
	 * 出生／重生时写入权威弹药。执行端：仅服务器，非权威调用直接返回。
	 * 同时复位换弹状态，保证新生命期不继承上一生命期的换弹进度。
	 */
	void InitializeAmmo(int32 InMagazineAmmo, int32 InReserveAmmo);

	/** 返回权威弹匣弹药。两端可读。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }

	/** 返回权威备用弹药。两端可读。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	//~End F05-01

public:

	//~Begin 为 F05-03/04/05 预留的只读状态访问（写入逻辑未实现，见 Docs/API/WeaponSystem.md）

	/** 是否正在换弹。F05-05 写入。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	bool IsReloading() const { return bIsReloading; }

	/** 换弹开始时的服务器时间（秒）。F05-05 写入，供客户端插值进度。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	double GetReloadStartServerTime() const { return ReloadStartServerTime; }

	//~End 预留状态

protected:

	/** 当前武器 Actor。 */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	/** 权威弹匣弹药。拥有者可见。 */
	UPROPERTY(Replicated)
	int32 MagazineAmmo = 0;

	/** 权威备用弹药。拥有者可见。 */
	UPROPERTY(Replicated)
	int32 ReserveAmmo = 0;

	/** 换弹状态占位。[F05-05 预留] 当前恒为 false。 */
	UPROPERTY(Replicated)
	bool bIsReloading = false;

	/** 换弹开始服务器时间占位。[F05-05 预留] 当前恒为 0。 */
	UPROPERTY(Replicated)
	double ReloadStartServerTime = 0.0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
