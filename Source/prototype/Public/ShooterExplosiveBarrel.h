// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterExplosiveBarrel.generated.h"


class UStaticMeshComponent;
class URadialForceComponent;
class UParticleSystem;
class USoundCue;


UCLASS()
class PROTOTYPE_API AShooterExplosiveBarrel : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AShooterExplosiveBarrel();

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	URadialForceComponent* RadialForceComp;

	UPROPERTY(ReplicatedUsing = OnRep_Exploded)
	bool bExploded;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float ExplosionDamage;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	UFUNCTION()
	void OnRep_Exploded();

	/* Impulse applied to the barrel mesh when it explodes to boost it up a little */
	UPROPERTY(EditDefaultsOnly, Category = "FX")
	float ExplosionImpulse;

	/* Particle to play when health reached zero */
	UPROPERTY(EditDefaultsOnly, Category = "FX")
	UParticleSystem* ExplosionEffect;

	/* The material to replace the original on the mesh once exploded (a blackened version) */
	UPROPERTY(EditDefaultsOnly, Category = "FX")
	UMaterialInterface* ExplodedMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	USoundCue* ExplosionSound;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Health", Replicated)
	float Health;

	/* Take damage & handle death */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
	class AActor* DamageCauser) override;

};