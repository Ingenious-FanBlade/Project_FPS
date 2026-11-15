// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleCharacter.h"
#include "Battle/BattleGameMode.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattlePlayerState.h"
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

//~End IShooterWeaponHolder interface

//~Begin F05-03 权威开火

const TCHAR* ABattleCharacter::GetFireRejectReasonText(EFireRejectReason Reason)
{
	switch (Reason)
	{
	case EFireRejectReason::None:          return TEXT("None");
	case EFireRejectReason::NoAuthority:   return TEXT("NoAuthority");
	case EFireRejectReason::RateLimited:   return TEXT("RateLimited");
	case EFireRejectReason::StaleRound:    return TEXT("StaleRound");
	case EFireRejectReason::PhaseDenied:   return TEXT("PhaseDenied");
	case EFireRejectReason::Dead:          return TEXT("Dead");
	case EFireRejectReason::NoWeapon:      return TEXT("NoWeapon");
	case EFireRejectReason::Reloading:     return TEXT("Reloading");
	case EFireRejectReason::NoAmmo:        return TEXT("NoAmmo");
	case EFireRejectReason::RefireRate:    return TEXT("RefireRate");
	case EFireRejectReason::DuplicateShot: return TEXT("DuplicateShot");
	case EFireRejectReason::BadAim:        return TEXT("BadAim");
	default:                               return TEXT("Unknown");
	}
}

double ABattleCharacter::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

bool ABattleCharacter::Server_Fire_Validate(
	int32 ClientRoundId, int32 ClientLifeId, int32 ShotId,
	FVector_NetQuantize AimViewOrigin, FVector_NetQuantizeNormal AimViewDir)
{
	// 只拦截将导致服务器异常或明显非法的值；返回 false 会断开连接。
	if (ClientRoundId < 0 || ClientLifeId < 0 || ShotId < 0)
	{
		return false;
	}

	if (AimViewOrigin.ContainsNaN() || AimViewDir.ContainsNaN())
	{
		return false;
	}

	// 世界坐标过大视为非法输入（1e7 cm 量级）。
	constexpr float MaxCoordinateSquared = 1.0e14f;
	return AimViewOrigin.SizeSquared() < MaxCoordinateSquared;
}

void ABattleCharacter::Server_Fire_Implementation(
	int32 ClientRoundId, int32 ClientLifeId, int32 ShotId,
	FVector_NetQuantize AimViewOrigin, FVector_NetQuantizeNormal AimViewDir)
{
	UE_LOG(LogTemp, Warning, TEXT("Server_Fire: Received on Server. ShotId=%d RoundId=%d LifeId=%d Authority=%s"),
		ShotId, ClientRoundId, ClientLifeId, HasAuthority() ? TEXT("yes") : TEXT("no"));

	const auto Reject = [this, ShotId](EFireRejectReason Reason)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: fire rejected ShotId=%d reason=%s"),
			*GetName(), ShotId, GetFireRejectReasonText(Reason));
	};

	// 1. 权威归属：只处理服务器侧。
	if (!HasAuthority())
	{
		Reject(EFireRejectReason::NoAuthority);
		return;
	}

	ABattlePlayerState* BattlePlayerState = GetPlayerState<ABattlePlayerState>();
	ABattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABattleGameMode>() : nullptr;
	if (!BattlePlayerState || !BattleGameMode)
	{
		Reject(EFireRejectReason::NoAuthority);
		return;
	}

	// 2. RPC 级限流：与玩法射速校验相互独立，先削减请求预算。
	const double ServerTime = GetServerTimeSeconds();
	if (LastRpcServerTime >= 0.0 && (ServerTime - LastRpcServerTime) < MinRpcInterval)
	{
		Reject(EFireRejectReason::RateLimited);
		return;
	}
	LastRpcServerTime = ServerTime;

	// 3. RoundId 校验：拒绝属于旧局的请求。
	const FBattleRoundIdentity RoundIdentity = BattleGameMode->GetRoundIdentity();
	UE_LOG(LogTemp, Warning, TEXT("Server expected RoundId: %d, got: %d"), RoundIdentity.RoundId, ClientRoundId);
	if (ClientRoundId != RoundIdentity.RoundId)
	{
		Reject(EFireRejectReason::StaleRound);
		return;
	}

	// 4. LifeId 校验：待 F06 接入后启用。当前客户端传 0 且不校验。
	// TODO(F06): 生成 LifeId 后比对 ClientLifeId 与当前生命期，旧生命期请求无效。
	(void)ClientLifeId;

	// 5. 阶段校验：复用 GameMode 的阶段许可。
	if (!BattleGameMode->CanAttack(BattlePlayerState))
	{
		Reject(EFireRejectReason::PhaseDenied);
		return;
	}

	// 6. 存活校验：F06 未实现，占位为存活。
	// TODO(F06): 改为读取权威存活状态。
	const bool bIsAlivePlaceholder = true;
	if (!bIsAlivePlaceholder)
	{
		Reject(EFireRejectReason::Dead);
		return;
	}

	// 7. 武器可用校验。
	if (!WeaponComponent || !IsValid(WeaponComponent->GetCurrentWeapon()))
	{
		Reject(EFireRejectReason::NoWeapon);
		return;
	}

	// 8. 换弹中禁止开火。字段由 F05-05 写入，当前恒为 false。
	if (WeaponComponent->IsReloading())
	{
		Reject(EFireRejectReason::Reloading);
		return;
	}

	// 9. 弹药校验。
	if (WeaponComponent->GetMagazineAmmo() <= 0)
	{
		Reject(EFireRejectReason::NoAmmo);
		return;
	}

	// 10. 射速校验：使用数据资产的射击间隔与服务器世界时间。
	const UBattleWeaponDefinition* Definition = GetWeaponDefinition();
	const double FireInterval = Definition ? static_cast<double>(Definition->FireInterval) : 0.0;
	const double LastFireTime = WeaponComponent->GetLastFireServerTime();
	if (FireInterval > 0.0 && LastFireTime >= 0.0 && (ServerTime - LastFireTime) < FireInterval)
	{
		Reject(EFireRejectReason::RefireRate);
		return;
	}

	// 11. ShotId 去重：只接受严格递增的编号，拦截重复与乱序旧包。
	if (ShotId <= WeaponComponent->GetLastAcceptedShotId())
	{
		Reject(EFireRejectReason::DuplicateShot);
		return;
	}

	// 12. 瞄准参数合理性：方向必须接近单位长度（容差 0.01）。
	if (!FMath::IsNearlyEqual(AimViewDir.SizeSquared(), 1.0, 0.01))
	{
		Reject(EFireRejectReason::BadAim);
		return;
	}

	// 13. 权威扣弹。失败说明状态在期间被改变，按无弹药处理。
	if (!WeaponComponent->ConsumeAmmo(1))
	{
		Reject(EFireRejectReason::NoAmmo);
		return;
	}

	// 14. 记录开火状态。
	WeaponComponent->RecordAcceptedShot(ShotId, ServerTime);

	UE_LOG(LogTemp, Warning, TEXT("Server_Fire: Fired! Ammo: %d"), WeaponComponent->GetMagazineAmmo());

	// 15. 表现通知（本阶段仅日志）。
	Multicast_FireEffect(ShotId, FVector_NetQuantize(GetActorLocation()));

	// 16. 留给 F05-04 的射线检测与伤害提交。
	PerformAuthoritativeShot(ShotId, AimViewOrigin, AimViewDir);
}

void ABattleCharacter::Multicast_FireEffect_Implementation(int32 ShotId, FVector_NetQuantize MuzzleLocation)
{
	// TODO(F05-04): 播放枪口、声音与开火动画；远端表现按 ShotId 去重。
	UE_LOG(LogProject_FPS, Log, TEXT("%s: fire effect ShotId=%d at %s (local=%s)"),
		*GetName(), ShotId, *MuzzleLocation.ToString(), IsLocallyControlled() ? TEXT("yes") : TEXT("no"));
}

void ABattleCharacter::PerformAuthoritativeShot(int32 ShotId, const FVector& ViewOrigin, const FVector& ViewDir)
{
	// TODO(F05-04): 在此执行服务器射线查询、命中筛选，并把伤害提交给 F06。
	// TODO(F05-04): 射线起点须由服务器角色／视点数据构造，参考 ViewOrigin 但不直接采用。
	(void)ShotId;
	(void)ViewOrigin;
	(void)ViewDir;
}

//~End F05-03

#if !UE_BUILD_SHIPPING

void ABattleCharacter::DebugTriggerFire()
{
	UE_LOG(LogTemp, Warning, TEXT("DebugTriggerFire: Character called. LocalControlled=%s Authority=%s"),
		IsLocallyControlled() ? TEXT("yes") : TEXT("no"),
		HasAuthority() ? TEXT("yes") : TEXT("no"));

	if (!IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugTriggerFire: not locally controlled, aborting."));
		return;
	}

	const int32 ShotId = DebugShotIdCounter++;

	// RoundId 必须读取复制的 GameState，而非 GameMode：
	// GameMode 只存在于服务器，客户端 GetAuthGameMode() 恒为 nullptr。
	// 与真实客户端一致：LifeId 待 F06 接入前传 0。
	const UWorld* World = GetWorld();
	const ABattleGameState* BattleGameState = World ? World->GetGameState<ABattleGameState>() : nullptr;
	if (!BattleGameState)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DebugTriggerFire: ABattleGameState not available yet (game state not replicated). Aborting."));
		return;
	}

	const int32 ClientRoundId = BattleGameState->GetRoundIdentity().RoundId;
	UE_LOG(LogTemp, Warning, TEXT("Client RoundId: %d"), ClientRoundId);

	// 未同步到有效 RoundId 时不发送请求，避免服务端按 StaleRound 拒绝或产生无意义流量。
	if (ClientRoundId <= 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DebugTriggerFire: RoundId not replicated yet (got %d). Wait for game state sync and retry."),
			ClientRoundId);
		return;
	}

	const FVector ViewOrigin = GetActorLocation();
	const FVector ViewDir = GetActorForwardVector();

	UE_LOG(LogTemp, Warning, TEXT("DebugTriggerFire: sending Server_Fire ShotId=%d RoundId=%d"), ShotId, ClientRoundId);

	// 客户端调用，由引擎路由到服务器。
	Server_Fire(ClientRoundId, 0, ShotId,
		FVector_NetQuantize(ViewOrigin), FVector_NetQuantizeNormal(ViewDir));
}

#endif
