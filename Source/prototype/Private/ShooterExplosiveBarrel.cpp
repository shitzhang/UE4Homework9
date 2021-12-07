// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterExplosiveBarrel.h"
#include "Components/ShooterHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundCue.h"


// Sets default values
AShooterExplosiveBarrel::AShooterExplosiveBarrel()
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetSimulatePhysics(true);
	// Set to physics body to let radial component affect us (eg. when a nearby barrel explodes)
	MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	RootComponent = MeshComp;

	RadialForceComp = CreateDefaultSubobject<URadialForceComponent>(TEXT("RadialForceComp"));
	RadialForceComp->SetupAttachment(MeshComp);
	RadialForceComp->Radius = 250;
	RadialForceComp->bImpulseVelChange = true;
	RadialForceComp->bAutoActivate = false; // Prevent component from ticking, and only use FireImpulse() instead
	RadialForceComp->bIgnoreOwningActor = true; // ignore self

	ExplosionImpulse = 400;

	SetReplicates(true);
	SetReplicateMovement(true);
}


void AShooterExplosiveBarrel::OnRep_Exploded()
{
	// Play FX and change self material to black
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation());
	// Override material on mesh with blackened version
	MeshComp->SetMaterial(0, ExplodedMaterial);
}

float AShooterExplosiveBarrel::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator,
	AActor* DamageCauser)
{
	if (bExploded)
	{
		// Nothing left to do, already exploded.
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		Health -= ActualDamage;

		if (Health <= 0)
		{
			// Explode!
			bExploded = true;
			OnRep_Exploded();

			// Boost the barrel upwards
			FVector BoostIntensity = FVector::UpVector * ExplosionImpulse;
			MeshComp->AddImpulse(BoostIntensity, NAME_None, true);

			// Blast away nearby physics actors
			RadialForceComp->FireImpulse();

			// @TODO: Apply radial damage

			TArray<AActor*> IgnoreActors;

			UGameplayStatics::ApplyRadialDamage(GetWorld(), ExplosionDamage, GetActorLocation(), RadialForceComp->Radius,
				DamageType,
				IgnoreActors, this, EventInstigator, true);

			if (ExplosionSound)
			{
				UGameplayStatics::SpawnSoundAtLocation(GetWorld(), ExplosionSound, GetActorLocation());
			}
		}
	}

	return ActualDamage;
}


void AShooterExplosiveBarrel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterExplosiveBarrel, bExploded);
}
