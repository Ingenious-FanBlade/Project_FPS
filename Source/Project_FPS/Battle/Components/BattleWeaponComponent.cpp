// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/Components/BattleWeaponComponent.h"
#include "ShooterWeapon.h"
#include "Net/UnrealNetwork.h"

UBattleWeaponComponent::UBattleWeaponComponent()
{
	// 弹药与换弹状态需要复制；组件随角色一起复制。
	SetIsReplicatedByDefault(true);

	// 作为挂接点使用：枪口与射线起点可在 F05-03 后挂接到本组件。
	PrimaryComponentTick.bCanEverTick = false;
}

void UBattleWeaponComponent::SetCurrentWeapon(AShooterWeapon* InWeapon)
{
	CurrentWeapon = InWeapon;
}

void UBattleWeaponComponent::InitializeAmmo(int32 InMagazineAmmo, int32 InReserveAmmo)
{
	// 只有服务器可以提交权威弹药。
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	MagazineAmmo = FMath::Max(0, InMagazineAmmo);
	ReserveAmmo = FMath::Max(0, InReserveAmmo);

	// 新生命期不继承上一生命期的换弹进度。
	bIsReloading = false;
	ReloadStartServerTime = 0.0;
}

void UBattleWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 弹药只对拥有者可见，避免向其他客户端泄露对手剩余弹药。
	DOREPLIFETIME_CONDITION(UBattleWeaponComponent, MagazineAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBattleWeaponComponent, ReserveAmmo, COND_OwnerOnly);

	// 换弹状态为占位字段，F05-05 接入；先按拥有者可见复制。
	DOREPLIFETIME_CONDITION(UBattleWeaponComponent, bIsReloading, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBattleWeaponComponent, ReloadStartServerTime, COND_OwnerOnly);
}
