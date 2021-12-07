// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterBaseCharacter.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShooterHealthComponent.h"
#include "Components/ShooterMovementComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Net/UnrealNetwork.h"
#include "ShooterDamageType.h"
#include "GameFramework/PlayerState.h"
#include "ShooterPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "World/ShooterGameMode.h"
#include "World/ShooterGameState.h"
#include "ShooterPowerupActor.h"
#include "Items/ShooterWeaponPickup.h"
#include "AI/ShooterVIPCharacter.h"


// Sets default values
AShooterBaseCharacter::AShooterBaseCharacter(const class FObjectInitializer& ObjectInitializer)
/* Override the movement class from the base class to our own to support multiple speeds (eg. sprinting) */
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	NoiseEmitterComp = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitterComp"));

	Health = 100;

	TargetingSpeedModifier = 0.5f;
	SprintingSpeedModifier = 2.5f;

	/* Don't collide with camera checks to keep 3rd person camera at position when zombies or other players are standing behind us */
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	LeftFootSocketName = "LeftFootSocket";
	RightFootSocketName = "RightFootSocket";

	RightFootArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("RightFootArrowComp"));
	RightFootArrowComp->SetupAttachment(GetMesh(), RightFootSocketName);

	LeftFootArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("LeftFootArrowComp"));
	LeftFootArrowComp->SetupAttachment(GetMesh(), LeftFootSocketName);

	ApplyDamageFactor = 1.0f;
	SuperSpeedFactor = 1.0f;

	PR_PowerUp = 0.4;

	PR_PickUpWeapon = 0.4;

	DefaultMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
}


float AShooterBaseCharacter::GetHealth() const
{
	return Health;
}


float AShooterBaseCharacter::GetMaxHealth() const
{
	// Retrieve the default value of the health property that is assigned on instantiation.
	return GetClass()->GetDefaultObject<AShooterBaseCharacter>()->Health;
}


bool AShooterBaseCharacter::IsAlive() const
{
	return Health > 0;
}


void AShooterBaseCharacter::SetSprinting(bool NewSprinting)
{
	bWantsToRun = NewSprinting;

	if (bIsCrouched)
	{
		UnCrouch();
	}

	if (!HasAuthority())
	{
		ServerSetSprinting(NewSprinting);
	}
}


void AShooterBaseCharacter::ServerSetSprinting_Implementation(bool NewSprinting)
{
	SetSprinting(NewSprinting);
}


bool AShooterBaseCharacter::ServerSetSprinting_Validate(bool NewSprinting)
{
	return true;
}


bool AShooterBaseCharacter::IsSprinting() const
{
	if (!GetCharacterMovement())
	{
		return false;
	}

	return bWantsToRun && !IsTargeting() && !GetVelocity().IsZero()
		// Don't allow sprint while strafing sideways or standing still (1.0 is straight forward, -1.0 is backward while near 0 is sideways or standing still)
		&& (FVector::DotProduct(GetVelocity().GetSafeNormal2D(), GetActorRotation().Vector()) > 0.8);
	// Changing this value to 0.1 allows for diagonal sprinting. (holding W+A or W+D keys)
}


float AShooterBaseCharacter::GetSprintingSpeedModifier() const
{
	return SprintingSpeedModifier;
}

void AShooterBaseCharacter::SetTargeting(bool NewTargeting)
{
	bIsTargeting = NewTargeting;

	if (!HasAuthority())
	{
		ServerSetTargeting(NewTargeting);
	}
}


void AShooterBaseCharacter::ServerSetTargeting_Implementation(bool NewTargeting)
{
	SetTargeting(NewTargeting);
}


bool AShooterBaseCharacter::ServerSetTargeting_Validate(bool NewTargeting)
{
	return true;
}


bool AShooterBaseCharacter::IsTargeting() const
{
	return bIsTargeting;
}


float AShooterBaseCharacter::GetTargetingSpeedModifier() const
{
	return TargetingSpeedModifier;
}


FRotator AShooterBaseCharacter::GetAimOffsets() const
{
	const FVector AimDirWS = GetBaseAimRotation().Vector();
	const FVector AimDirLS = ActorToWorld().InverseTransformVectorNoScale(AimDirWS);
	const FRotator AimRotLS = AimDirLS.Rotation();

	return AimRotLS;
}


// Called when the game starts or when spawned
void AShooterBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Cast<AShooterVIPCharacter>(this))
	{
		AShooterGameState* MyGameState = Cast<AShooterGameState>(GetWorld()->GetGameState());
		if (MyGameState)
		{
			MyGameState->VIPHealth = Health;
		}
	}
}


float AShooterBaseCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent,
                                        class AController* EventInstigator, class AActor* DamageCauser)
{
	if (Health <= 0.f)
	{
		return 0.f;
	}

	/* Modify based on gametype rules */
	AShooterGameMode* MyGameMode = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode());
	Damage = MyGameMode ? MyGameMode->ModifyDamage(Damage, this, DamageEvent, EventInstigator, DamageCauser) : Damage;

	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (ActualDamage > 0.f)
	{
		Health -= ActualDamage;

		if (Cast<AShooterVIPCharacter>(this))
		{
			AShooterGameState* MyGameState = Cast<AShooterGameState>(GetWorld()->GetGameState());
			if (MyGameState)
			{
				MyGameState->VIPHealth = Health;
			}
		}

		if (Health <= 0)
		{
			bool bCanDie = true;

			/* Check the damagetype, always allow dying if the cast fails, otherwise check the property if player can die from damagetype */
			if (DamageEvent.DamageTypeClass)
			{
				UShooterDamageType* DmgType = Cast<UShooterDamageType>(DamageEvent.DamageTypeClass->GetDefaultObject());
				bCanDie = (DmgType == nullptr || (DmgType && DmgType->GetCanDieFrom()));
			}

			if (bCanDie)
			{
				Die(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
			}
			else
			{
				/* Player cannot die from this damage type, set hitpoints to 1.0 */
				Health = 1.0f;
			}
		}
		else
		{
			/* Shorthand for - if x != null pick1 else pick2 */
			APawn* Pawn = EventInstigator ? EventInstigator->GetPawn() : nullptr;
			PlayHit(ActualDamage, DamageEvent, Pawn, DamageCauser, false);
		}
	}

	return ActualDamage;
}

void AShooterBaseCharacter::Heal(float HealAmount)
{
	if (HealAmount <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health + HealAmount, 0.0f, GetMaxHealth());

	UE_LOG(LogTemp, Log, TEXT("Health Changed: %s (+%s)"), *FString::SanitizeFloat(Health),
	       *FString::SanitizeFloat(HealAmount));
}


bool AShooterBaseCharacter::CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer,
                                   AActor* DamageCauser) const
{
	/* Check if character is already dying, destroyed or if we have authority */
	if (bDied ||
		IsPendingKill() ||
		!HasAuthority() ||
		GetWorld()->GetAuthGameMode() == NULL)
	{
		return false;
	}

	return true;
}


void AShooterBaseCharacter::FellOutOfWorld(const class UDamageType& DmgType)
{
	Die(Health, FDamageEvent(DmgType.GetClass()), NULL, NULL);
}


bool AShooterBaseCharacter::Die(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer,
                                AActor* DamageCauser)
{
	if (!CanDie(KillingDamage, DamageEvent, Killer, DamageCauser))
	{
		return false;
	}

	Health = FMath::Min(0.0f, Health);

	/* Fallback to default DamageType if none is specified */
	UDamageType const* const DamageType = DamageEvent.DamageTypeClass
		                                      ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>()
		                                      : GetDefault<UDamageType>();
	Killer = GetDamageInstigator(Killer, *DamageType);

	/* Notify the gamemode we got killed for scoring and game over state */
	AController* KilledPlayer = Controller ? Controller : Cast<AController>(GetOwner());
	GetWorld()->GetAuthGameMode<AShooterGameMode>()->Killed(Killer, KilledPlayer, this, DamageType);

	// Spawn Power Up
	auto PS = GetPlayerState();

	if (PS->IsABot())
	{
		if (PowerUpClasses.Num() > 0)
		{
			float PR = FMath::RandRange(0.0f, 1.0f);
			if (PR < PR_PowerUp)
			{
				int32 PowerUpIdx = FMath::RandRange(0, PowerUpClasses.Num() - 1);

				UE_LOG(LogTemp, Log, TEXT("PowerUpIdx: %s"), *FString::FromInt(PowerUpIdx));

				auto PowerUpClass = PowerUpClasses[PowerUpIdx];

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				FVector SpawnLocation = FVector(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z - 60);

				GetWorld()->SpawnActor<AShooterPowerupActor>(PowerUpClass, SpawnLocation, GetActorRotation(),
				                                             SpawnParams);
			}
		}

		if (PickUpWeaponClasses.Num() > 0)
		{
			float PR = FMath::RandRange(0.0f, 1.0f);
			if (PR < PR_PickUpWeapon)
			{
				int32 PickUpWeaponIdx = FMath::RandRange(0, PickUpWeaponClasses.Num() - 1);

				UE_LOG(LogTemp, Log, TEXT("PickUpWeaponIdx: %s"), *FString::FromInt(PickUpWeaponIdx));

				auto PickUpWeaponClass = PickUpWeaponClasses[PickUpWeaponIdx];

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				FVector SpawnLocation = FVector(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);

				AShooterWeaponPickup* NewWeaponPickup = GetWorld()->SpawnActor<AShooterWeaponPickup>(
					PickUpWeaponClass, SpawnLocation, GetActorRotation(),
					SpawnParams);

				if (NewWeaponPickup)
				{
					/* Apply torque to make it spin when dropped. */
					UStaticMeshComponent* MeshComp = NewWeaponPickup->GetMeshComponent();
					if (MeshComp)
					{
						MeshComp->SetSimulatePhysics(true);
						MeshComp->AddTorqueInRadians(FVector(1, 1, 1) * 4000000);
					}
				}
			}
		}
	}

	OnDeath(KillingDamage, DamageEvent, Killer ? Killer->GetPawn() : NULL, DamageCauser);
	return true;
}


void AShooterBaseCharacter::OnDeath(float KillingDamage, FDamageEvent const& DamageEvent, APawn* PawnInstigator,
                                    AActor* DamageCauser)
{
	if (bDied)
	{
		return;
	}

	SetReplicateMovement(true);
	TearOff();
	bDied = true;

	PlayHit(KillingDamage, DamageEvent, PawnInstigator, DamageCauser, true);

	DetachFromControllerPendingDestroy();

	/* Disable all collision on capsule */
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	USkeletalMeshComponent* Mesh3P = GetMesh();
	if (Mesh3P)
	{
		Mesh3P->SetCollisionProfileName(TEXT("Ragdoll"));
	}
	SetActorEnableCollision(true);

	SetRagdollPhysics();

	/* Apply physics impulse on the bone of the enemy skeleton mesh we hit (ray-trace damage only) */
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		FPointDamageEvent PointDmg = *((FPointDamageEvent*)(&DamageEvent));
		{
			// TODO: Use DamageTypeClass->DamageImpulse
			Mesh3P->AddImpulseAtLocation(PointDmg.ShotDirection * 30000, PointDmg.HitInfo.ImpactPoint,
			                             PointDmg.HitInfo.BoneName);
		}
	}
	if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		FRadialDamageEvent RadialDmg = *((FRadialDamageEvent const*)(&DamageEvent));
		{
			Mesh3P->AddRadialImpulse(RadialDmg.Origin, RadialDmg.Params.GetMaxRadius(),
			                         100000 /*RadialDmg.DamageTypeClass->DamageImpulse*/,
			                         ERadialImpulseFalloff::RIF_Linear);
		}
	}
}


void AShooterBaseCharacter::SetRagdollPhysics()
{
	bool bInRagdoll = false;
	USkeletalMeshComponent* Mesh3P = GetMesh();

	if (IsPendingKill())
	{
		bInRagdoll = false;
	}
	else if (!Mesh3P || !Mesh3P->GetPhysicsAsset())
	{
		bInRagdoll = false;
	}
	else
	{
		Mesh3P->SetAllBodiesSimulatePhysics(true);
		Mesh3P->SetSimulatePhysics(true);
		Mesh3P->WakeAllRigidBodies();
		Mesh3P->bBlendPhysics = true;

		bInRagdoll = true;
	}

	UCharacterMovementComponent* CharacterComp = Cast<UCharacterMovementComponent>(GetMovementComponent());
	if (CharacterComp)
	{
		CharacterComp->StopMovementImmediately();
		CharacterComp->DisableMovement();
		CharacterComp->SetComponentTickEnabled(false);
	}

	if (!bInRagdoll)
	{
		// Immediately hide the pawn
		TurnOff();
		SetActorHiddenInGame(true);
		SetLifeSpan(1.0f);
	}
	else
	{
		SetLifeSpan(10.0f);
	}
}


void AShooterBaseCharacter::PlayHit(float DamageTaken, struct FDamageEvent const& DamageEvent, APawn* PawnInstigator,
                                    AActor* DamageCauser, bool bKilled)
{
	if (HasAuthority())
	{
		ReplicateHit(DamageTaken, DamageEvent, PawnInstigator, DamageCauser, bKilled);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bKilled && SoundDeath)
		{
			UGameplayStatics::SpawnSoundAttached(SoundDeath, RootComponent, NAME_None, FVector::ZeroVector,
			                                     EAttachLocation::SnapToTarget, true);
		}
		else if (SoundTakeHit)
		{
			UGameplayStatics::SpawnSoundAttached(SoundTakeHit, RootComponent, NAME_None, FVector::ZeroVector,
			                                     EAttachLocation::SnapToTarget, true);
		}
	}
}


void AShooterBaseCharacter::ReplicateHit(float DamageTaken, struct FDamageEvent const& DamageEvent,
                                         APawn* PawnInstigator, AActor* DamageCauser, bool bKilled)
{
	const float TimeoutTime = GetWorld()->GetTimeSeconds() + 0.5f;

	FDamageEvent const& LastDamageEvent = LastTakeHitInfo.GetDamageEvent();
	if (PawnInstigator == LastTakeHitInfo.PawnInstigator.Get() && LastDamageEvent.DamageTypeClass == LastTakeHitInfo.
		DamageTypeClass)
	{
		// Same frame damage
		if (bKilled && LastTakeHitInfo.bKilled)
		{
			// Redundant death take hit, ignore it
			return;
		}

		DamageTaken += LastTakeHitInfo.ActualDamage;
	}

	LastTakeHitInfo.ActualDamage = DamageTaken;
	LastTakeHitInfo.PawnInstigator = Cast<AShooterBaseCharacter>(PawnInstigator);
	LastTakeHitInfo.DamageCauser = DamageCauser;
	LastTakeHitInfo.SetDamageEvent(DamageEvent);
	LastTakeHitInfo.bKilled = bKilled;
	LastTakeHitInfo.EnsureReplication();
}


void AShooterBaseCharacter::OnRep_LastTakeHitInfo()
{
	if (LastTakeHitInfo.bKilled)
	{
		OnDeath(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(),
		        LastTakeHitInfo.DamageCauser.Get());
	}
	else
	{
		PlayHit(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(),
		        LastTakeHitInfo.DamageCauser.Get(), LastTakeHitInfo.bKilled);
	}
}

void AShooterBaseCharacter::OnRep_SuperSpeedFactor()
{
	UE_LOG(LogTemp, Log, TEXT("MaxWalkSpeed: %s"), *FString::SanitizeFloat(GetCharacterMovement()->MaxWalkSpeed));
}

void AShooterBaseCharacter::UpdateMaxWalkSpeed()
{
	if(!HasAuthority())
	{
		ServerUpdateMaxWalkSpeed();
	}

	GetCharacterMovement()->MaxWalkSpeed = DefaultMaxWalkSpeed * SuperSpeedFactor;
}


void AShooterBaseCharacter::ServerUpdateMaxWalkSpeed_Implementation()
{
	UpdateMaxWalkSpeed();
}

bool AShooterBaseCharacter::ServerUpdateMaxWalkSpeed_Validate()
{
	return true;
}


void AShooterBaseCharacter::TraceFootprint(FHitResult& OutHit, const FVector& Location) const
{
	FVector Start = Location;
	FVector End = Location;

	Start.Z += 20.0f;
	End.Z -= 20.0f;

	//Re-initialize hit info
	OutHit = FHitResult(ForceInit);

	FCollisionQueryParams TraceParams(FName(TEXT("Footprint trace")), true, this);
	TraceParams.bReturnPhysicalMaterial = true;

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, TraceParams);
}

void AShooterBaseCharacter::RightFootDown() const
{
	SpawnFootprint(RightFootArrowComp, RightFootprintDecal);
}


void AShooterBaseCharacter::LeftFootDown() const
{
	SpawnFootprint(LeftFootArrowComp, LeftFootprintDecal);
}

void AShooterBaseCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	AShooterCharacter* OtherChar = Cast<AShooterCharacter>(OtherActor);
	if(OtherChar && OtherChar->GetPlayerState())
	{
		if(!OtherChar->GetPlayerState()->IsABot() && OtherChar->bPendingPunch)
		{
			OtherChar->ServerPerformPunchDamage(this);
		}
	}
}


void AShooterBaseCharacter::SpawnFootprint(UArrowComponent* FootArrow, TSubclassOf<AActor> FootprintDecal) const
{
	FHitResult HitResult;
	FVector FootWorldPosition = FootArrow->GetComponentTransform().GetLocation();
	FVector Forward = FootArrow->GetForwardVector();

	TraceFootprint(HitResult, FootWorldPosition);

	// Create a rotator using the landscape normal and our foot forward vectors
	// Note that we use the function ZX to enforce the normal direction (Z)
	FQuat floorRot = FRotationMatrix::MakeFromZX(HitResult.Normal, Forward).ToQuat();
	//FQuat offsetRot(FRotator(0.0f, -90.0f, 0.0f));
	FRotator Rotation = (floorRot).Rotator();

	// Spawn decal and particle emitter
	if (FootprintDecal)
		AActor* DecalInstance = GetWorld()->SpawnActor(FootprintDecal, &HitResult.Location, &Rotation);
}


void AShooterBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Value is already updated locally, skip in replication step
	DOREPLIFETIME_CONDITION(AShooterBaseCharacter, bWantsToRun, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AShooterBaseCharacter, bIsTargeting, COND_SkipOwner);

	// Replicate to every client, no special condition required
	DOREPLIFETIME(AShooterBaseCharacter, Health);
	DOREPLIFETIME(AShooterBaseCharacter, LastTakeHitInfo);
	DOREPLIFETIME(AShooterBaseCharacter, ApplyDamageFactor);
	DOREPLIFETIME(AShooterBaseCharacter, SuperSpeedFactor);
}
