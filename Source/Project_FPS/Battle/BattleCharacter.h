// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Project_FPSCharacter.h"
#include "ShooterWeaponHolder.h"
#include "BattleCharacter.generated.h"

class AShooterWeapon;
class UAnimMontage;
class UBattleWeaponComponent;
class UBattleWeaponDefinition;
class UInputAction;
class UInputComponent;

/**
 * 正式对战角色（F04／F05 的宿主）。
 *
 * 基类为 AProject_FPSCharacter，避免继承性模板角色的重生计时器、队伍字节与
 * 计分调用；武器持有能力通过 IShooterWeaponHolder 在本类实现。
 *
 * F05-01 实现：开启复制，出生时（仅服务器）按 UBattleWeaponDefinition 生成武器
 * 并提交权威初始弹药；弹药存于 UBattleWeaponComponent。开火（F05-03）、
 * 射线与伤害（F05-04）、换弹（F05-05）未实现。
 */
UCLASS(config = Game)
class PROJECT_FPS_API ABattleCharacter : public AProject_FPSCharacter, public IShooterWeaponHolder
{
	GENERATED_BODY()

public:

	/** Constructor */
	ABattleCharacter();

protected:

	/** 射击输入动作（复用 Content/Variant_Shooter/Input/Actions/IA_Shoot）。F05-03 绑定。 */
	UPROPERTY(EditAnywhere, Category = "Battle|Input")
	UInputAction* FireAction;

	/** 出生装备的武器配置。为空时不发枪。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Weapon")
	TObjectPtr<UBattleWeaponDefinition> WeaponDefinition;

	/** 武器组件：持有当前武器与权威弹药。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Weapon")
	TObjectPtr<UBattleWeaponComponent> WeaponComponent;

	/** 激活武器时写入的 AI 感知噪声标签，与模板 Player 标签语义一致。 */
	UPROPERTY(EditAnywhere, Category = "Battle|Weapon")
	FName WeaponNoiseOwnerTag = FName("Player");

protected:

	/** 出生装备：仅服务器按配置生成武器并初始化弹药。 */
	virtual void BeginPlay() override;

	/** 绑定射击输入。F05-03 接入开火请求入口。 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 按当前配置生成并装备武器。执行端：仅服务器。 */
	void EquipFromDefinition();

	/** 返回出生装备使用的数据资产，可能为空。 */
	const UBattleWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

public:

	//~Begin F05-03 权威开火

	/**
	 * 客户端开火请求。不可靠 Server RPC，仅由拥有者角色提交。
	 *
	 * @param ClientRoundId 客户端认为的当前 RoundId，服务器须与 GameState 比对。
	 * @param ClientLifeId  客户端认为的当前 LifeId。**待 F06 接入后启用校验**，
	 *                      当前客户端传 0 且服务器不校验，保留参数以固定签名。
	 * @param ShotId        本生命期内单调递增的射击序号，用于去重。
	 * @param AimViewOrigin 客户端视点起点，仅作合理性参考，不作为射线起点交由服务器查询。
	 * @param AimViewDir    客户端瞄准方向，用于限流与参数合理性校验。
	 */
	UFUNCTION(Server, Unreliable)
	void Server_Fire(int32 ClientRoundId, int32 ClientLifeId, int32 ShotId, FVector_NetQuantize AimViewOrigin, FVector_NetQuantizeNormal AimViewDir);

	/** Server_Fire 的参数合法性检查。返回 false 会断连接，因此只拦截明显非法值。 */
	bool Server_Fire_Validate(int32 ClientRoundId, int32 ClientLifeId, int32 ShotId, FVector_NetQuantize AimViewOrigin, FVector_NetQuantizeNormal AimViewDir);

	/** 开火成功的表现通知。本阶段仅记录日志，待 F05-04／F05-02 接入表现。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_FireEffect(int32 ShotId, FVector_NetQuantize MuzzleLocation);

	//~End F05-03

protected:

	/** 开火拒绝原因，用于日志与后续诊断统计。 */
	enum class EFireRejectReason : uint8
	{
		None,
		NoAuthority,
		RateLimited,
		StaleRound,
		PhaseDenied,
		Dead,
		NoWeapon,
		Reloading,
		NoAmmo,
		RefireRate,
		DuplicateShot,
		BadAim,
	};

	/** 返回拒绝原因的可读文本。 */
	static const TCHAR* GetFireRejectReasonText(EFireRejectReason Reason);

	/** 返回服务器世界时间（秒）。 */
	double GetServerTimeSeconds() const;

	/** 完成开火后的权威射击处理。执行端：仅服务器。实现射线检测，伤害提交待 F06。 */
	virtual void PerformAuthoritativeShot(int32 ShotId, const FVector& ViewOrigin, const FVector& ViewDir);

	/** 返回服务器侧射线起点，不采用客户端坐标。 */
	FVector GetServerShotOrigin() const;

	/** 返回服务器侧射线方向，不采用客户端坐标。 */
	FVector GetServerShotDirection() const;

	/** RPC 级最小间隔（秒）。与玩法射速校验相互独立。 */
	UPROPERTY(EditDefaultsOnly, Category = "Battle|Fire")
	float MinRpcInterval = 0.05f;

	/** 最近一次处理开火 RPC 的服务器时间，用于请求限流。 */
	double LastRpcServerTime = -1.0;

public:

	/** 返回武器组件，始终有效。 */
	UFUNCTION(BlueprintPure, Category = "Battle|Weapon")
	UBattleWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

public:

	/**
	 * 本地调试：由拥有者客户端触发一次开火请求，走完整 Server_Fire 校验链。
	 *
	 * 仅供 ABattlePlayerController::BattleTestFire 调用；实现仅在非 Shipping
	 * 构建中提供（见 BattleCharacter.cpp）。Shipping 构建中调用会链接失败。
	 */
	void DebugTriggerFire();

private:

	/** 本地调试用的 ShotId 计数器。 */
	int32 DebugShotIdCounter = 1;

public:

	//~Begin IShooterWeaponHolder interface
	// F05-01 实现 AttachWeaponMeshes／AddWeaponClass／OnWeaponActivated／OnWeaponDeactivated。
	// 其余接口按 TODO 标注由 F05-02/03/04 实现，签名与接口声明不得改动。

	/** 将武器的第一／第三人称网格挂接到本角色。 */
	virtual void AttachWeaponMeshes(AShooterWeapon* Weapon) override;

	/** TODO(S3-F05-02): 播放开火动画。 */
	virtual void PlayFiringMontage(UAnimMontage* Montage) override;

	/** TODO(S3-F05-02): 应用武器后坐力。 */
	virtual void AddWeaponRecoil(float Recoil) override;

	/** TODO(S3-F05-04): 用权威弹药刷新 Battle HUD。 */
	virtual void UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize) override;

	/** TODO(S3-F05-04): 返回服务器限定的瞄准位置，不信任客户端任意坐标。 */
	virtual FVector GetWeaponTargetLocation() override;

	/** 出生装备入口：由数据资产驱动生成并激活武器。执行端：仅服务器。 */
	virtual void AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass) override;

	/** 记录当前武器并写入出生弹药。执行端：服务器写入弹药，两端记录引用。 */
	virtual void OnWeaponActivated(AShooterWeapon* Weapon) override;

	/** 停止当前武器的开火状态。 */
	virtual void OnWeaponDeactivated(AShooterWeapon* Weapon) override;

	/** TODO(S3-F05-03): 半自动冷却，允许再次开火。 */
	virtual void OnSemiWeaponRefire() override;

	//~End IShooterWeaponHolder interface
};
