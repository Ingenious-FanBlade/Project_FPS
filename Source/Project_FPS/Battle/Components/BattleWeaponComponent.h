// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BattleWeaponComponent.generated.h"

class AShooterWeapon;

/** 拥有者客户端收到权威弹药或换弹状态后触发。 */
DECLARE_MULTICAST_DELEGATE(FOnBattleWeaponStateChanged);

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

	/**
	 * 权威武器状态变化通知。服务器提交状态及拥有者客户端 RepNotify 都会触发。
	 * 仅传递“状态已变化”，订阅方应通过只读访问器获取同一帧完整快照。
	 */
	FOnBattleWeaponStateChanged OnWeaponStateChanged;

	//~End F05-01

public:

	//~Begin F05-03 权威开火使用的服务器状态

	/**
	 * 权威扣除弹匣弹药。执行端：仅服务器，非权威调用直接返回 false。
	 * 只扣 MagazineAmmo，不修改 ReserveAmmo，也不触发换弹（换弹属 F05-05）。
	 * @return 成功扣除返回 true；数量非法或弹药不足返回 false 且不做修改。
	 */
	bool ConsumeAmmo(int32 Amount);

	/** 返回服务器记录的最近一次开火时间（世界时间秒）。仅服务器有意义。 */
	double GetLastFireServerTime() const { return LastFireServerTime; }

	/** 记录一次被接受的射击：写入开火时间与 ShotId。执行端：仅服务器。 */
	void RecordAcceptedShot(int32 ShotId, double ServerTime);

	/** 返回服务器已接受的最大 ShotId，用于去重。仅服务器有意义。 */
	int32 GetLastAcceptedShotId() const { return LastAcceptedShotId; }

	/** 新生命期开始时复位服务器开火状态。执行端：仅服务器。 */
	void ResetFireState();

	//~End F05-03

public:

	//~Begin F05-05 换弹的服务器提交接口

	/**
	 * 标记换弹开始。执行端：仅服务器，非权威调用直接返回 false。
	 * 不判断是否可换弹，只提交状态；条件判断由 ABattleCharacter 的校验链负责。
	 */
	bool BeginReload(double ServerTime);

	/**
	 * 完成换弹：按 LoadAmount 从备用弹药转移到弹匣，并清除换弹状态。
	 * 执行端：仅服务器，非权威调用直接返回 false。
	 * @return 实际转移的子弹数；调用方应保证已通过服务端预计算。
	 */
	int32 CompleteReload(int32 LoadAmount);

	/** 中断换弹：清除换弹状态，不转移任何弹药。执行端：仅服务器。 */
	void CancelReload();

	/** 返回服务器已接受的最大换弹请求编号，用于幂等去重。-1 表示本生命期尚未换弹。 */
	int32 GetLastAcceptedReloadId() const { return LastAcceptedReloadId; }

	/** 记录一次被接受的换弹请求编号。执行端：仅服务器。 */
	void RecordAcceptedReload(int32 ReloadRequestId);

	//~End F05-05

public:

	//~Begin 换弹状态只读访问（F05-05 写入，客户端读取用于表现）

	/** 是否正在换弹。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	bool IsReloading() const { return bIsReloading; }

	/** 换弹开始时的服务器时间（秒）。供客户端插值进度。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	double GetReloadStartServerTime() const { return ReloadStartServerTime; }

	//~End 换弹状态

protected:

	/** 当前武器 Actor。 */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> CurrentWeapon;

	/** 权威弹匣弹药。拥有者可见。 */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState)
	int32 MagazineAmmo = 0;

	/** 权威备用弹药。拥有者可见。 */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState)
	int32 ReserveAmmo = 0;

	/** 换弹状态。由 F05-05 的 BeginReload／CompleteReload／CancelReload 写入。 */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState)
	bool bIsReloading = false;

	/** 换弹开始时的服务器时间（秒）。换弹结束时清零。 */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState)
	double ReloadStartServerTime = 0.0;

	/** 最近一次被接受的开火时间（世界时间秒）。不复制，仅服务器使用。 */
	double LastFireServerTime = -1.0;

	/** 已接受的最大 ShotId。-1 表示本生命期尚未开火。不复制，仅服务器使用。 */
	int32 LastAcceptedShotId = -1;

	/** 已接受的最大换弹请求编号。-1 表示本生命期尚未换弹。不复制，仅服务器使用。 */
	int32 LastAcceptedReloadId = -1;

	/** 任一拥有者可见武器状态复制完成后的统一通知入口。 */
	UFUNCTION()
	void OnRep_WeaponState();

	/** 广播状态变化；不修改任何权威数据。 */
	void NotifyWeaponStateChanged();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
