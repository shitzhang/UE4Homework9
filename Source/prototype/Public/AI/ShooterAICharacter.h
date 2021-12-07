// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShooterCharacter.h"
#include "ShooterAICharacter.generated.h"


class AShooterWeapon;

/**
 * 
 */
UCLASS()
class PROTOTYPE_API AShooterAICharacter : public AShooterCharacter
{
	GENERATED_BODY()

public:
	AShooterAICharacter(const class FObjectInitializer& ObjectInitializer);

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AShooterWeapon> DefaultWeaponClass;

	virtual void BeginPlay() override;
};
