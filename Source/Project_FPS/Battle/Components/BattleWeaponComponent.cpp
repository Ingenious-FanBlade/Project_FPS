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

	// 新生命期重新开始射速、ShotId 与换弹请求编号记录。
	ResetFireState();
	LastAcceptedReloadId = -1;
}

bool UBattleWeaponComponent::ConsumeAmmo(int32 Amount)
{
	// 只有服务器可以扣除权威弹药。
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// 防御：换弹期间不可扣弹。开火校验链已有第 8 步拦截，
	// 此处保证未来任何绕过校验链的调用路径也不会在换弹中扣弹。
	if (bIsReloading)
	{
		return false;
	}

	// 非法数量或弹药不足时不做任何修改。
	if (Amount <= 0 || MagazineAmmo < Amount)
	{
		return false;
	}

	MagazineAmmo -= Amount;
	return true;
}

bool UBattleWeaponComponent::BeginReload(double ServerTime)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	bIsReloading = true;
	ReloadStartServerTime = ServerTime;
	return true;
}

int32 UBattleWeaponComponent::CompleteReload(int32 LoadAmount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return 0;
	}

	// 只允许转移有效数量；非法或不足时不修改任何字段。
	if (LoadAmount <= 0 || LoadAmount > ReserveAmmo)
	{
		bIsReloading = false;
		ReloadStartServerTime = 0.0;
		return 0;
	}

	MagazineAmmo += LoadAmount;
	ReserveAmmo -= LoadAmount;

	bIsReloading = false;
	ReloadStartServerTime = 0.0;

	return LoadAmount;
}

void UBattleWeaponComponent::CancelReload()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// 中断不转移任何弹药，保持原子性。
	bIsReloading = false;
	ReloadStartServerTime = 0.0;
}

void UBattleWeaponComponent::RecordAcceptedReload(int32 ReloadRequestId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	LastAcceptedReloadId = ReloadRequestId;
}

void UBattleWeaponComponent::RecordAcceptedShot(int32 ShotId, double ServerTime)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	LastFireServerTime = ServerTime;
	LastAcceptedShotId = ShotId;
}

void UBattleWeaponComponent::ResetFireState()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	LastFireServerTime = -1.0;
	LastAcceptedShotId = -1;
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
