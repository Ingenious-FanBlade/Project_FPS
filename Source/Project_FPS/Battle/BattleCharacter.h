// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_FPSCharacter.h"
#include "ShooterWeaponHolder.h"
#include "BattleCharacter.generated.h"

class AShooterWeapon;
class UAnimMontage;
class UInputAction;
class UInputComponent;

/**
 * 正式对战角色（F04／F05 的宿主）。
 *
 * 基类为 AProject_FPSCharacter，避免继承性模板角色的重生计时器、队伍字节与
 * 计分调用；武器持有能力通过 IShooterWeaponHolder 在本类实现。
 *
 * S1 状态（F08-01 M1）：本类只保证可实例化，9 个武器接口均为占位空体，
 * 由 S3（F05-01 起）实现。本类 S1 阶段不开启 bReplicates，S3 在 F05-01
 * 设置复制并在 Docs/API 同步。
 */
UCLASS(config = Game)
class PROJECT_FPS_API ABattleCharacter : public AProject_FPSCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABattleCharacter();

protected:

	/** 射击输入动作（复用 Content/Variant_Shooter/Input/Actions/IA_Shoot）。S3 绑定。 */
	UPROPERTY(EditAnywhere, Category = "Battle|Input")
	UInputAction* FireAction;

	/** 当前持有的武器。S3（F05-01）负责出生装备与切换。 */
	UPROPERTY(Transient)
	TObjectPtr<AShooterWeapon> CurrentWeapon;

protected:

	/** 绑定射击输入。S3 接入开火请求入口。 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:

	//~Begin IShooterWeaponHolder interface
	// S1：以下 9 个函数仅为占位实现，保证类可实例化；
	// S3 依次按 TODO 标注的职责填充，签名与接口声明不得改动。

	/** TODO(S3-F05-01): 挂接武器双网格。 */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** TODO(S3-F05-02): 播放开火动画。 */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** TODO(S3-F05-02): 应用武器后坐力。 */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** TODO(S3-F05-04): 用权威弹药刷新 Battle HUD。 */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** TODO(S3-F05-04): 返回服务器否定的瞄准位置，不信任客户端任意坐标。 */
	virtual FVector GetWeaponTargetLocation() override;

	/** TODO(S3-F05-01): 出生装备入口（数据资产驱动的武器授予）。 */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** TODO(S3-F05-01): 武器激活回调与 HUD 刷新。 */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** TODO(S3-F05-01): 武器停用回调。 */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** TODO(S3-F05-03): 半自动冷却，允许再次开火。 */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface

public:

	/** 当前持有的武器，S1 恒为空。 */
	AShooterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }
};
