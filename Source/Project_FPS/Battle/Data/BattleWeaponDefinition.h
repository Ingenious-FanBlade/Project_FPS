// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleWeaponDefinition.generated.h"

class AShooterWeapon;
class UAnimInstance;
class UAnimMontage;

/**
 * 步枪配置数据资产（F05-01）。
 *
 * 服务端与客户端共用同一资产：战斗数值由服务端读取并提交为权威状态，
 * 表现引用由客户端读取。数值基线见玩法设计「战斗数值与判定」。
 *
 * 约束：本资产只描述参数，不含运行期状态。弹匣、备用弹药等运行期权威数据
 * 由 UBattleWeaponComponent 持有，不写回本资产。
 */
UCLASS(BlueprintType)
class PROJECT_FPS_API UBattleWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** 界面显示名称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Display")
	FText DisplayName;

#pragma region Combat

	/** 单发伤害。玩法设计基线为 25，首版不区分头部倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 0.0, ClampMax = 1000.0))
	float DamagePerShot = 25.0f;

	/** 射击间隔（秒）。玩法设计基线为 0.1 秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 0.01, ClampMax = 5.0, Units = "s"))
	float FireInterval = 0.1f;

	/** 弹匣容量。玩法设计基线为 30。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 1, ClampMax = 300))
	int32 MagazineSize = 30;

	/** 出生时的备用弹药。玩法设计基线为 90。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 0, ClampMax = 600))
	int32 InitialReserveAmmo = 90;

	/** 换弹耗时（秒）。玩法设计基线为 2 秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 0.1, ClampMax = 10.0, Units = "s"))
	float ReloadDuration = 2.0f;

	/** 最大射击距离（厘米）。用于服务端射线范围限制。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat", meta = (ClampMin = 100.0, ClampMax = 100000.0, Units = "cm"))
	float MaxRange = 10000.0f;

	/** 是否全自动。步枪默认为全自动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Combat")
	bool bFullAuto = true;

#pragma endregion Combat

#pragma region Presentation

	/** 要生成的武器 Actor 类，通常指向模板武器蓝图（如 BP_ShooterWeapon_Rifle）。仅服务器在装备时加载。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	TSoftClassPtr<AShooterWeapon> WeaponClass;

	/** 第一人称动画蓝图类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> FirstPersonAnimInstanceClass;

	/** 第三人称动画蓝图类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> ThirdPersonAnimInstanceClass;

	/** 开火蒙太奇。F05-02 使用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	TSoftObjectPtr<UAnimMontage> FiringMontage;

	/** 第一人称武器挂接插槽。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	FName FirstPersonWeaponSocket = FName("HandGrip_R");

	/** 第三人称武器挂接插槽。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battle Weapon|Presentation")
	FName ThirdPersonWeaponSocket = FName("HandGrip_R");

#pragma endregion Presentation

public:

	/** 绑定是否可用于出生装备：武器类必须配置。 */
	UFUNCTION(BlueprintPure, Category = "Battle Weapon")
	bool IsEquippable() const { return !WeaponClass.IsNull(); }
};
