// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleCharacter.h"
#include "EnhancedInputComponent.h"
#include "ShooterWeapon.h"

ABattleCharacter::ABattleCharacter()
{
	// S1 只提供可实例化的正式角色；移动、镜头与第一人称网格由基类提供。
	// 武器持有、输入绑定和 HUD 联动由 S3（F05）实现。
}

void ABattleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 基类负责移动、观察与跳跃的绑定。
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// TODO(S3-F05-03): 绑定 FireAction 到服务器开火请求入口。
}

//~Begin IShooterWeaponHolder interface
// S1：以下均为占位空体，保证 AProject_FPSCharacter 派生的角色可实例化。
// 每一步的职责见 BattleCharacter.h 中的 TODO(S3-*) 标注。

void ABattleCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	// TODO(S3-F05-01): 将武器的第一／第三人称网格挂接到本角色的对应插槽。
}

void ABattleCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	// TODO(S3-F05-02): 播放武器的开火蒙太奇。
}

void ABattleCharacter::AddWeaponRecoil(float Recoil)
{
	// TODO(S3-F05-02): 将后坐力作为俯仰输入作用于控制器。
}

void ABattleCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	// TODO(S3-F05-04): 将权威弹药推送到 ABattlePlayerController 的 HUD 接口。
}

FVector ABattleCharacter::GetWeaponTargetLocation()
{
	// TODO(S3-F05-04): 由服务器用角色／视点数据构造有界射线终点。
	return FVector::ZeroVector;
}

void ABattleCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// TODO(S3-F05-01): 生成并装备数据资产指定的武器，初始化弹匣与备用弹药。
}

void ABattleCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// TODO(S3-F05-01): 记录当前武器并刷新弹药显示。
}

void ABattleCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// TODO(S3-F05-01): 停止当前武器的开火状态。
}

void ABattleCharacter::OnSemiWeaponRefire()
{
	// TODO(S3-F05-03): 通知武器冷却结束，可再次接受开火请求。
}

//~End IShooterWeaponHolder interface
