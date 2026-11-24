// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battle/BattleCharacter.h"
#include "Battle/BattleGameMode.h"
#include "Battle/BattleGameState.h"
#include "Battle/BattlePlayerState.h"
#include "Battle/BattleTypes.h"
#include "Battle/Components/BattleWeaponComponent.h"
#include "Battle/Data/BattleWeaponDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "Net/UnrealNetwork.h"
#include "Project_FPS.h"
#include "ShooterWeapon.h"
#include "TimerManager.h"

ABattleCharacter::ABattleCharacter()
{
	// 角色需要复制到客户端，武器与弹药状态随角色下发。
	bReplicates = true;

	// 武器组件：挂接到根组件，作为枪口与射线起点的稳定父节点。
	WeaponComponent = CreateDefaultSubobject<UBattleWeaponComponent>(TEXT("Battle Weapon"));
	WeaponComponent->SetupAttachment(RootComponent);

	// 初始为正常行走速度。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// 网络更新频率显式化，便于调参和排查（此前全部使用引擎默认值）。
	// 使用 setter 以消除 C4996（直接赋值已标记废弃）。
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(30.0f);
	NetPriority = 3.0f;
}

void ABattleCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 订阅比赛阶段变化，阶段切换时重新评估移动能力（不依赖 Tick 轮询）。
	if (UWorld* World = GetWorld())
	{
		if (ABattleGameState* BattleGameState = World->GetGameState<ABattleGameState>())
		{
			BoundGameState = BattleGameState;
			BattleGameState->OnMatchPhaseChanged.AddDynamic(this, &ABattleCharacter::HandleMatchPhaseChanged);
		}
	}

	// 出生装备与初始移动许可只在服务器执行；角色重建（重生）时会再次触发。
	if (HasAuthority())
	{
		EquipFromDefinition();
		ApplySprintSpeed();
		UpdateMovementPermission();

		// 建立防作弊位移基准，避免出生首帧被误判为瞬移。
		RecordMovementBaseline();
	}
}

void ABattleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 方案②以 C++ 类直接启动，没有蓝图实例可配置基类的输入动作；
	// 若不在绑定前赋值，基类 BindAction 会收到 nullptr 并静默跳过，表现为无法移动与观察。
	if (!MoveAction)
	{
		MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	}
	if (!LookAction)
	{
		LookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	}
	if (!MouseLookAction)
	{
		MouseLookAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	}
	if (!JumpAction)
	{
		JumpAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	}

	if (!MoveAction || !LookAction || !MouseLookAction || !JumpAction)
	{
		UE_LOG(LogProject_FPS, Error, TEXT("%s: failed to load default input actions from /Game/Input/Actions."), *GetName());
	}

	// 冲刺动作复用 Horror 模板资产，避免新增资产或蓝图配置。
	if (!SprintAction)
	{
		SprintAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Variant_Horror/Input/IA_Sprint.IA_Sprint"));
	}
	if (!SprintAction)
	{
		UE_LOG(LogProject_FPS, Warning, TEXT("%s: failed to load IA_Sprint; sprint input will not bind."), *GetName());
	}

	// 基类负责移动、观察与跳跃的绑定。
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 冲刺绑定：按下/松开分别提交服务器请求。
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IsValid(SprintAction))
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ABattleCharacter::Input_StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABattleCharacter::Input_StopSprint);
		}
	}

	// TODO(S3-F02/F05-02): 绑定 FireAction 到本地开火流程。
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
	// 正式 HUD 从 UBattleWeaponComponent 的拥有者复制状态刷新；模板武器回调
	// 不含备用弹药，不能作为正式战斗弹药真值。
	(void)CurrentAmmo;
	(void)MagazineSize;
}

float ABattleCharacter::GetWeaponReloadDuration() const
{
	return WeaponDefinition ? FMath::Max(0.1f, WeaponDefinition->ReloadDuration) : 0.0f;
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
	case EFireRejectReason::AlreadyReloading: return TEXT("AlreadyReloading");
	case EFireRejectReason::MagazineFull:     return TEXT("MagazineFull");
	case EFireRejectReason::NoReserveAmmo:    return TEXT("NoReserveAmmo");
	case EFireRejectReason::Sprinting:        return TEXT("Sprinting");
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

	// 11. 冲刺互斥：冲刺中拒绝开火（M1 取舍：玩家须先松开冲刺键）。
	//     放在射速之后、ShotId 去重之前，避免无效请求占用去重编号。
	if (bIsSprinting)
	{
		Reject(EFireRejectReason::Sprinting);
		return;
	}

	// 12. ShotId 去重：只接受严格递增的编号，拦截重复与乱序旧包。
	if (ShotId <= WeaponComponent->GetLastAcceptedShotId())
	{
		Reject(EFireRejectReason::DuplicateShot);
		return;
	}

	// 13. 瞄准参数合理性：方向必须接近单位长度（容差 0.01）。
	if (!FMath::IsNearlyEqual(AimViewDir.SizeSquared(), 1.0, 0.01))
	{
		Reject(EFireRejectReason::BadAim);
		return;
	}

	// 14. 权威扣弹。失败说明状态在期间被改变，按无弹药处理。
	if (!WeaponComponent->ConsumeAmmo(1))
	{
		Reject(EFireRejectReason::NoAmmo);
		return;
	}

	// 15. 记录开火状态。
	WeaponComponent->RecordAcceptedShot(ShotId, ServerTime);

	UE_LOG(LogTemp, Warning, TEXT("Server_Fire: Fired! Ammo: %d"), WeaponComponent->GetMagazineAmmo());

	// 16. 表现通知（本阶段仅日志）。
	Multicast_FireEffect(ShotId, FVector_NetQuantize(GetActorLocation()));

	// 17. 留给 F05-04 的射线检测与伤害提交。
	PerformAuthoritativeShot(ShotId, AimViewOrigin, AimViewDir);
}

void ABattleCharacter::Multicast_FireEffect_Implementation(int32 ShotId, FVector_NetQuantize MuzzleLocation)
{
	// TODO(F05-04): 播放枪口、声音与开火动画；远端表现按 ShotId 去重。
	UE_LOG(LogProject_FPS, Log, TEXT("%s: fire effect ShotId=%d at %s (local=%s)"),
		*GetName(), ShotId, *MuzzleLocation.ToString(), IsLocallyControlled() ? TEXT("yes") : TEXT("no"));
}

FVector ABattleCharacter::GetServerShotOrigin() const
{
	// 设计约束（03-server.md:102）：射击起点由服务器构造，不采用客户端坐标。
	// 优先使用服务器上的第一人称相机；Dedicated Server 无渲染时退化为角色位置。
	if (const UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		return Camera->GetComponentLocation();
	}

	return GetActorLocation();
}

FVector ABattleCharacter::GetServerShotDirection() const
{
	// 方向同样取服务器侧：优先控制旋转，其次角色朝向。
	if (GetController())
	{
		return GetController()->GetControlRotation().Vector();
	}

	return GetActorForwardVector();
}

void ABattleCharacter::PerformAuthoritativeShot(int32 ShotId, const FVector& ViewOrigin, const FVector& ViewDir)
{
	// 执行端保护：本函数只在服务器侧有效。
	if (!HasAuthority())
	{
		return;
	}

	// 客户端瞄准参数仅作参考，不参与命中判定；此处只记录偏差供诊断。
	(void)ViewOrigin;
	(void)ViewDir;

	const UBattleWeaponDefinition* Definition = GetWeaponDefinition();
	const float BaseDamage = Definition ? Definition->DamagePerShot : 25.0f;
	const float MaxRange = Definition ? Definition->MaxRange : 10000.0f;

	FBattleShotResult Result;
	Result.ShotId = ShotId;
	Result.Shooter = this;
	Result.BaseDamage = BaseDamage;
	Result.FinalDamage = BaseDamage;

	const ABattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABattleGameMode>() : nullptr;
	Result.RoundId = BattleGameMode ? BattleGameMode->GetRoundIdentity().RoundId : 0;

	// 起点与方向均取自服务器，忽略客户端传入值。
	const FVector Start = GetServerShotOrigin();
	const FVector Direction = GetServerShotDirection().GetSafeNormal();
	const FVector End = Start + Direction * MaxRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BattleAuthoritativeShot), false, this);
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

	Result.bBlockingHit = bHit;
	Result.HitLocation = bHit ? Hit.ImpactPoint : End;
	Result.HitBoneName = bHit ? Hit.BoneName : NAME_None;

	if (!bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("PerformShot ShotId=%d: no hit (range %.0f)"), ShotId, MaxRange);
		return;
	}

	ABattleCharacter* HitCharacter = Cast<ABattleCharacter>(Hit.GetActor());
	if (!HitCharacter || HitCharacter == this)
	{
		// 命中环境或非战斗对象：本阶段不产生伤害。
		UE_LOG(LogTemp, Warning, TEXT("PerformShot ShotId=%d: hit non-battle actor %s at %s"),
			ShotId, *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString());
		return;
	}

	Result.Victim = HitCharacter;

	const ABattlePlayerState* ShooterPlayerState = GetPlayerState<ABattlePlayerState>();
	const ABattlePlayerState* VictimPlayerState = HitCharacter->GetPlayerState<ABattlePlayerState>();

	// TeamId 仅用于日志核对；友军过滤属 F06-02，本阶段不做拦截。
	const int32 ShooterTeam = ShooterPlayerState ? static_cast<int32>(ShooterPlayerState->GetTeam()) : 0;
	const int32 VictimTeam = VictimPlayerState ? static_cast<int32>(VictimPlayerState->GetTeam()) : 0;

	// LifeId 待 F06 生成；当前恒为 0。
	// TODO(F06): 填充 Result.VictimLifeId = HitCharacter 的当前生命期序号。
	Result.VictimLifeId = 0;

	UE_LOG(LogTemp, Warning,
		TEXT("PerformShot ShotId=%d RoundId=%d: hit %s (ShooterTeam=%d VictimTeam=%d) damage=%.1f at %s bone=%s"),
		ShotId, Result.RoundId, *GetNameSafe(HitCharacter), ShooterTeam, VictimTeam,
		Result.FinalDamage, *Hit.ImpactPoint.ToString(), *Result.HitBoneName.ToString());

	// TODO(F06): 在此调用 F06 的统一伤害提交接口，参数为 Result。
	//            具体要求：伤害入口负责存活/保护/友军过滤，并只结算一次死亡。
	UE_LOG(LogTemp, Warning,
		TEXT("PerformShot ShotId=%d: TODO(F06) damage submission not implemented; no health changed."),
		ShotId);
}

//~End F05-03

//~Begin F05-05 换弹

void ABattleCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// 解绑阶段变化，避免角色销毁后旧回调。
	if (BoundGameState)
	{
		BoundGameState->OnMatchPhaseChanged.RemoveDynamic(this, &ABattleCharacter::HandleMatchPhaseChanged);
		BoundGameState = nullptr;
	}

	// 清理防作弊位移基准，避免角色重建后沿用过期的合法位置。
	bHasMovementBaseline = false;

	// 角色销毁（死亡、重生、切图）时清理换弹计时器与状态，避免旧计时器继续执行。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}

	// 递增代次，任何仍在排队的回调都会因代次不匹配而失效。
	++ReloadGeneration;

	if (HasAuthority() && WeaponComponent && WeaponComponent->IsReloading())
	{
		WeaponComponent->CancelReload();
	}

	Super::EndPlay(EndPlayReason);
}

bool ABattleCharacter::Server_Reload_Validate(int32 ClientRoundId, int32 ClientLifeId, int32 ReloadRequestId)
{
	// 只拦截会导致服务器异常的值；返回 false 会断开连接。
	return ClientRoundId >= 0 && ClientLifeId >= 0 && ReloadRequestId >= 0;
}

void ABattleCharacter::Server_Reload_Implementation(int32 ClientRoundId, int32 ClientLifeId, int32 ReloadRequestId)
{
	const auto Reject = [this, ReloadRequestId](EFireRejectReason Reason)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: reload rejected RequestId=%d reason=%s"),
			*GetName(), ReloadRequestId, GetFireRejectReasonText(Reason));
	};

	// 1. 权威归属。
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

	// 2. 幂等去重：同一或更旧的请求编号直接忽略，避免重复开始换弹。
	//    可靠 RPC 可能重复投递，这里静默丢弃，不视为错误。
	if (WeaponComponent && ReloadRequestId <= WeaponComponent->GetLastAcceptedReloadId())
	{
		return;
	}

	// 3. RoundId 校验。
	const FBattleRoundIdentity RoundIdentity = BattleGameMode->GetRoundIdentity();
	if (ClientRoundId != RoundIdentity.RoundId)
	{
		Reject(EFireRejectReason::StaleRound);
		return;
	}

	// 4. LifeId 校验：待 F06 接入后启用。
	// TODO(F06): 生成 LifeId 后比对 ClientLifeId 与当前生命期。
	(void)ClientLifeId;

	// 5. 阶段校验：复用与开火相同的阶段许可。
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

	// 8. 已在换弹：不实现主动取消，重复请求一律拒绝。
	if (WeaponComponent->IsReloading())
	{
		Reject(EFireRejectReason::AlreadyReloading);
		return;
	}

	// 9. 弹匣已满。
	const UBattleWeaponDefinition* Definition = GetWeaponDefinition();
	const int32 MagazineSize = Definition ? Definition->MagazineSize : 30;
	if (WeaponComponent->GetMagazineAmmo() >= MagazineSize)
	{
		Reject(EFireRejectReason::MagazineFull);
		return;
	}

	// 10. 备用弹药不足。
	if (WeaponComponent->GetReserveAmmo() <= 0)
	{
		Reject(EFireRejectReason::NoReserveAmmo);
		return;
	}

	// 全部通过：提交换弹开始、记录请求编号并启动计时。
	const double ServerTime = GetServerTimeSeconds();
	const float ReloadDuration = Definition ? FMath::Max(0.1f, Definition->ReloadDuration) : 2.0f;

	WeaponComponent->BeginReload(ServerTime);
	WeaponComponent->RecordAcceptedReload(ReloadRequestId);

	ActiveReloadRequestId = ReloadRequestId;
	const int32 CurrentGeneration = ++ReloadGeneration;

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &ABattleCharacter::OnReloadCompleted, CurrentGeneration);
		World->GetTimerManager().SetTimer(ReloadTimer, Delegate, ReloadDuration, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("Server_Reload: started RequestId=%d duration=%.2f Ammo=%d/%d"),
		ReloadRequestId, ReloadDuration, WeaponComponent->GetMagazineAmmo(), WeaponComponent->GetReserveAmmo());

	Multicast_ReloadEffect(ReloadRequestId, true, ServerTime);
}

void ABattleCharacter::OnReloadCompleted(int32 Generation)
{
	// 过期计时器：期间发生过中断或新一轮换弹，直接丢弃。
	if (Generation != ReloadGeneration)
	{
		return;
	}

	if (!HasAuthority() || !WeaponComponent)
	{
		return;
	}

	// 防御：状态可能已被其他路径清理。
	if (!WeaponComponent->IsReloading())
	{
		return;
	}

	const UBattleWeaponDefinition* Definition = GetWeaponDefinition();
	const int32 MagazineSize = Definition ? Definition->MagazineSize : 30;

	// 换满为止；备弹不足时抽干备弹。
	const int32 NeededAmount = FMath::Max(0, MagazineSize - WeaponComponent->GetMagazineAmmo());
	const int32 LoadAmount = FMath::Min(NeededAmount, WeaponComponent->GetReserveAmmo());

	WeaponComponent->CompleteReload(LoadAmount);

	UE_LOG(LogTemp, Warning, TEXT("Reload Completed: Mag=%d, Reserve=%d"),
		WeaponComponent->GetMagazineAmmo(), WeaponComponent->GetReserveAmmo());

	UE_LOG(LogTemp, Warning, TEXT("Server_Reload: completed RequestId=%d loaded=%d Ammo=%d/%d"),
		ActiveReloadRequestId, LoadAmount, WeaponComponent->GetMagazineAmmo(), WeaponComponent->GetReserveAmmo());

	Multicast_ReloadEffect(ActiveReloadRequestId, false, 0.0);
	ActiveReloadRequestId = -1;
}

void ABattleCharacter::InterruptReload(const FString& Reason)
{
	if (!HasAuthority() || !WeaponComponent || !WeaponComponent->IsReloading())
	{
		return;
	}

	// 递增代次使任何排队的完成回调失效。
	++ReloadGeneration;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}

	// 中断不转移任何弹药，保持原子性。
	WeaponComponent->CancelReload();

	UE_LOG(LogTemp, Warning, TEXT("%s: reload interrupted RequestId=%d reason=%s"),
		*GetName(), ActiveReloadRequestId, *Reason);

	Multicast_ReloadEffect(ActiveReloadRequestId, false, 0.0);
	ActiveReloadRequestId = -1;
}

void ABattleCharacter::Multicast_ReloadEffect_Implementation(int32 ReloadRequestId, bool bStarted, double ServerStartTime)
{
	// TODO(F05-02): 播放换弹动画与音效，按 ReloadRequestId 去重；进度用 ServerStartTime 与数据资产时长插值。
	UE_LOG(LogTemp, Warning, TEXT("%s: reload effect RequestId=%d started=%s startTime=%.2f"),
		*GetName(), ReloadRequestId, bStarted ? TEXT("true") : TEXT("false"), ServerStartTime);
}

//~End F05-05

//~Begin F04-04 冲刺与移动门控

void ABattleCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 冲刺状态需要让所有客户端看到（远端角色表现）。
	DOREPLIFETIME(ABattleCharacter, bIsSprinting);
}

void ABattleCharacter::Input_StartSprint()
{
	// 客户端只提交意图；是否接受由服务器校验链决定。
	if (!HasAuthority())
	{
		// 本地预测：立即生效，服务器确认或拒绝后再纠正。
		bLocalSprintPredicted = true;
		ApplySprintSpeed();

		// 去抖：意图未变化时不重复发送 RPC，避免连按造成状态抖动。
		if (LastSentSprintIntent != 1)
		{
			LastSentSprintIntent = 1;
			Server_SetSprint(true);
		}
		return;
	}

	Server_SetSprint_Implementation(true);
}

void ABattleCharacter::Input_StopSprint()
{
	if (!HasAuthority())
	{
		bLocalSprintPredicted = false;
		ApplySprintSpeed();

		if (LastSentSprintIntent != 0)
		{
			LastSentSprintIntent = 0;
			Server_SetSprint(false);
		}
		return;
	}

	Server_SetSprint_Implementation(false);
}

void ABattleCharacter::Server_SetSprint_Implementation(bool bWantsSprint)
{
	// 1. 权威归属。
	if (!HasAuthority())
	{
		return;
	}

	if (!bWantsSprint)
	{
		// 关闭冲刺无条件接受：保证任何校验失败后玩家仍能恢复状态。
		if (bIsSprinting)
		{
			bIsSprinting = false;
			ApplySprintSpeed();
		}
		return;
	}

	// 2. 阶段许可：不允许移动的阶段也不允许冲刺。
	const ABattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABattleGameMode>() : nullptr;
	const ABattlePlayerState* BattlePlayerState = GetPlayerState<ABattlePlayerState>();
	if (!BattleGameMode || !BattlePlayerState || !BattleGameMode->CanMove(BattlePlayerState))
	{
		// 拒绝时主动通知拥有者回滚本地预测（不依赖 RepNotify，因为状态未变化不会触发）。
		Client_SetSprintRejected();
		return;
	}

	// 3. 换弹中不允许冲刺（与武器动作互斥）。
	if (WeaponComponent && WeaponComponent->IsReloading())
	{
		Client_SetSprintRejected();
		return;
	}

	if (!bIsSprinting)
	{
		bIsSprinting = true;
		ApplySprintSpeed();
	}
}

void ABattleCharacter::Client_SetSprintRejected_Implementation()
{
	// 只影响本地预测，权威状态以服务器为准。
	bLocalSprintPredicted = false;

	// 允许玩家松手后再次尝试。
	LastSentSprintIntent = -1;

	ApplySprintSpeed();

	UE_LOG(LogTemp, Warning, TEXT("%s: sprint request rejected by server; local prediction rolled back."), *GetName());
}

void ABattleCharacter::OnRep_IsSprinting()
{
	// 客户端：冲刺状态变化后同步本地速度表现。
	// 本地预测与权威不一致时，以权威为准纠正。
	if (IsLocallyControlled())
	{
		bLocalSprintPredicted = bIsSprinting;
	}

	ApplySprintSpeed();
}

void ABattleCharacter::ApplySprintSpeed()
{
	// 速度写入的唯一入口，保证服务器与客户端一致。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 拥有者客户端使用本地预测值以获得即时反馈；服务器与远端使用权威复制值。
		const bool bEffectiveSprint = (!HasAuthority() && IsLocallyControlled())
			? bLocalSprintPredicted
			: bIsSprinting;

		Movement->MaxWalkSpeed = bEffectiveSprint ? SprintSpeed : WalkSpeed;
	}
}

void ABattleCharacter::HandleMatchPhaseChanged(EBattleMatchPhase NewPhase)
{
	// 两端都更新本地表现；权威的移动禁用只在服务器执行。
	ApplySprintSpeed();

	if (HasAuthority())
	{
		UpdateMovementPermission();
	}
}

void ABattleCharacter::UpdateMovementPermission()
{
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ABattleGameMode* BattleGameMode = World ? World->GetAuthGameMode<ABattleGameMode>() : nullptr;
	const ABattlePlayerState* BattlePlayerState = GetPlayerState<ABattlePlayerState>();

	// 无法取得比赛状态时不做限制，避免因初始化顺序误禁用移动。
	if (!BattleGameMode || !BattlePlayerState)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (BattleGameMode->CanMove(BattlePlayerState))
	{
		// 恢复移动能力。仅当当前处于被禁用状态时才重置，避免打断空中状态。
		if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		ApplySprintSpeed();
	}
	else
	{
		// 阶段不允许移动：立即停止并禁用，不依赖客户端自觉。
		if (bIsSprinting)
		{
			bIsSprinting = false;
			ApplySprintSpeed();
		}
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

//~End F04-04

//~Begin F04-02 服务端移动同步与防作弊

void ABattleCharacter::RecordMovementBaseline()
{
	if (!HasAuthority())
	{
		return;
	}

	LastKnownGoodLocation = GetActorLocation();
	bHasMovementBaseline = true;
}

void ABattleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 仅供服务器做位移校验；客户端不做该判定。
	if (!HasAuthority() || !bHasMovementBaseline)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// 移动被禁用或角色未落地时跳过：此时位置本不应变化，垂直掉落不计入水平校验。
	if (Movement->MovementMode == MOVE_None)
	{
		RecordMovementBaseline();
		return;
	}

	// 卡顿帧跳过校验，避免单帧 DeltaTime 过大造成误判。
	if (DeltaSeconds <= 0.0f || DeltaSeconds > MaxMovementCheckDeltaTime)
	{
		RecordMovementBaseline();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();

	// 只比较水平 2D 距离，排除跳跃、掉落与台阶带来的垂直位移。
	const FVector HorizontalDelta(CurrentLocation.X - LastKnownGoodLocation.X, CurrentLocation.Y - LastKnownGoodLocation.Y, 0.0);
	const float MovedDistance = HorizontalDelta.Size();

	// 理论上限：以角色当前最大行走速度为准，叠加倍率与固定容差。
	const float MaxAllowedSpeed = FMath::Max(Movement->MaxWalkSpeed, SprintSpeed);
	const float AllowedDistance = MaxAllowedSpeed * MovementSpeedToleranceRatio * DeltaSeconds + MovementToleranceCms;

	if (MovedDistance <= AllowedDistance)
	{
		RecordMovementBaseline();
		return;
	}

	// 异常加速或瞬移：立即拉回上一次合法位置。
	UE_LOG(LogTemp, Warning,
		TEXT("AntiCheat: %s moved %.1f cm in %.3f s (allowed %.1f cm, speed %.0f). Reverting to %s."),
		*GetName(), MovedDistance, DeltaSeconds, AllowedDistance, MaxAllowedSpeed, *LastKnownGoodLocation.ToString());

	SetActorLocation(LastKnownGoodLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* MutableMovement = GetCharacterMovement())
	{
		MutableMovement->StopMovementImmediately();
	}

	// 修正后立即刷新基准，避免下一帧继续判违规。
	RecordMovementBaseline();
}

//~End F04-02

void ABattleCharacter::DebugTeleport(float Distance)
{
#if UE_BUILD_SHIPPING
	// Shipping 构建不提供调试能力。
	(void)Distance;
#else
	UE_LOG(LogTemp, Warning, TEXT("DebugTeleport: Character called. LocalControlled=%s Authority=%s Distance=%.0f"),
		IsLocallyControlled() ? TEXT("yes") : TEXT("no"),
		HasAuthority() ? TEXT("yes") : TEXT("no"),
		Distance);

	// 只要求满足“控制该角色的本端”，服务器主机（Listen Server 的 Server 窗口）同样允许，
	// 因为服务器位置也需要被位移校验覆盖。
	if (!IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugTeleport: this command targets the locally controlled character only."));
		return;
	}

	// 在本地移动角色：客户端上会由服务器 Tick 拉回；服务器上则被自身 Tick 拉回。
	const FVector NewLocation = GetActorLocation() + GetActorForwardVector() * Distance;
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogTemp, Warning, TEXT("DebugTeleport: moved %.0f cm to %s; expecting server revert."),
		Distance, *NewLocation.ToString());
#endif
}

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

void ABattleCharacter::DebugTriggerReload()
{
	UE_LOG(LogTemp, Warning, TEXT("DebugTriggerReload: Character called. LocalControlled=%s Authority=%s"),
		IsLocallyControlled() ? TEXT("yes") : TEXT("no"),
		HasAuthority() ? TEXT("yes") : TEXT("no"));

	if (!IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugTriggerReload: not locally controlled, aborting."));
		return;
	}

	const int32 ReloadRequestId = ++DebugReloadRequestIdCounter;

	// 与开火一致：RoundId 取自复制的 GameState，LifeId 待 F06 接入前传 0。
	const UWorld* World = GetWorld();
	const ABattleGameState* BattleGameState = World ? World->GetGameState<ABattleGameState>() : nullptr;
	if (!BattleGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugTriggerReload: ABattleGameState not available yet. Aborting."));
		return;
	}

	const int32 ClientRoundId = BattleGameState->GetRoundIdentity().RoundId;
	UE_LOG(LogTemp, Warning, TEXT("DebugTriggerReload: Client RoundId: %d RequestId=%d"), ClientRoundId, ReloadRequestId);

	if (ClientRoundId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DebugTriggerReload: RoundId not replicated yet (got %d)."), ClientRoundId);
		return;
	}

	Server_Reload(ClientRoundId, 0, ReloadRequestId);
}

#endif
