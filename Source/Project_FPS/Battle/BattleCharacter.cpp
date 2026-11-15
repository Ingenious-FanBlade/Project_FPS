// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleCharacter.h"
#include "Battle/Components/BattleWeaponComponent.h"
#include "Battle/Data/BattleWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Project_FPS.h"
#include "ShooterWeapon.h"

ABattleCharacter::ABattleCharacter()
{
	// 角色需要复制到客户端，武器与弹药状态随角色下发。
	bReplicates = true;

	// 武器组件：挂接到根组件，作为枪口与射线起点的稳定父节点。
	WeaponComponent = CreateDefaultSubobject<UBattleWeaponComponent>(TEXT("Battle Weapon"));
	WeaponComponent->SetupAttachment(RootComponent);
}

void ABattleCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 出生装备只在服务器执行；角色重建（重生）时会再次触发。
	if (HasAuthority())
	{
		EquipFromDefinition();
	}
}

void ABattleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 基类负责移动、观察与跳跃的绑定。
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// TODO(S3-F05-03): 绑定 FireAction 到服务器开火请求入口。
}

void ABattleCharacter::EquipFromDefinition()
{
	if (!WeaponComponent)
	{
		UE_LOG(LogProject_FPS, Error, TEXT("%s: WeaponComponent is missing."), *GetName());
		return;
	}

	if (!WeaponDefinition)
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("%s: no WeaponDefinition configured; no weapon equipped."), *GetName());
		return;
	}

	if (!WeaponDefinition->IsEquippable())
	{
		UE_LOG(LogProject_FPS, Error, TEXT("%s: WeaponDefinition %s has no WeaponClass."),
			*GetName(), *GetNameSafe(WeaponDefinition));
		return;
	}

	// 已有武器时不重复装备（重入保护）。
	if (IsValid(WeaponComponent->GetCurrentWeapon()))
	{
		return;
	}

	// 仅在服务器加载武器类；客户端表现引用按需加载。
	UClass* LoadedWeaponClass = WeaponDefinition->WeaponClass.LoadSynchronous();
	if (!LoadedWeaponClass)
	{
		UE_LOG(LogProject_FPS, Error, TEXT("%s: failed to load WeaponClass from %s."),
			*GetName(), *GetNameSafe(WeaponDefinition));
		return;
	}

	AddWeaponClass(LoadedWeaponClass);
}

void ABattleCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	if (!IsValid(Weapon))
	{
		return;
	}

	const UBattleWeaponDefinition* Definition = GetWeaponDefinition();

	const FName FirstPersonSocket = Definition ? Definition->FirstPersonWeaponSocket : FName("HandGrip_R");
	const FName ThirdPersonSocket = Definition ? Definition->ThirdPersonWeaponSocket : FName("HandGrip_R");

	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// 武器 Actor 挂到武器组件，保持相对武器组件的稳定变换；
	// 组件缺失时退化挂到角色本身，避免空指针。
	if (WeaponComponent)
	{
		Weapon->AttachToComponent(WeaponComponent, AttachmentRule);
	}
	else
	{
		Weapon->AttachToActor(this, AttachmentRule);
	}

	// 第一人称武器网格只对拥有者可见；第三人称网格对他人可见。
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonSocket);

	// 此处不记录 CurrentWeapon：武器 BeginPlay 早于角色 AddWeaponClass 的激活步骤，
	// 引用统一由 AddWeaponClass 与 OnWeaponActivated 提交。
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
	// 武器生成属于服务器权威行为，客户端不得自行授予武器。
	if (!HasAuthority())
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("%s: AddWeaponClass ignored on non-authority."), *GetName());
		return;
	}

	if (!WeaponClass || !WeaponComponent)
	{
		return;
	}

	// 单武器：已有武器时先停用并清理引用，再按新类装备（M1 不维护多武器列表）。
	if (AShooterWeapon* ExistingWeapon = WeaponComponent->GetCurrentWeapon())
	{
		ExistingWeapon->DeactivateWeapon();
		WeaponComponent->SetCurrentWeapon(nullptr);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

	AShooterWeapon* SpawnedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);
	if (!SpawnedWeapon)
	{
		UE_LOG(LogProject_FPS, Error, TEXT("%s: failed to spawn weapon %s."), *GetName(), *GetNameSafe(WeaponClass));
		return;
	}

	// 注意：SpawnActor 已同步执行武器 BeginPlay，AttachWeaponMeshes 与
	// WeaponOwner 解析均发生在该阶段；此处再激活并记录到组件。
	WeaponComponent->SetCurrentWeapon(SpawnedWeapon);
	SpawnedWeapon->ActivateWeapon(WeaponNoiseOwnerTag);
}

void ABattleCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	if (!Weapon || !WeaponComponent)
	{
		return;
	}

	WeaponComponent->SetCurrentWeapon(Weapon);

	// 服务器按数据资产写入权威弹药；客户端只等待复制。
	if (HasAuthority())
	{
		if (const UBattleWeaponDefinition* Definition = GetWeaponDefinition())
		{
			WeaponComponent->InitializeAmmo(Definition->MagazineSize, Definition->InitialReserveAmmo);
		}
	}
}

void ABattleCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	(void)Weapon;

	// 停用时不重置弹药：弹药属于生命期状态，由重生时重新初始化。
}

void ABattleCharacter::OnSemiWeaponRefire()
{
	// TODO(S3-F05-03): 通知武器冷却结束，可再次接受开火请求。
}
