// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ShooterCharacter.h"
#include "ShooterVIPCharacter.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPE_API AShooterVIPCharacter : public AShooterCharacter
{
	GENERATED_BODY()

public:
	AShooterVIPCharacter(const class FObjectInitializer& ObjectInitializer);

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AShooterWeapon> DefaultWeaponClass;

	virtual void BeginPlay() override;
	
};
