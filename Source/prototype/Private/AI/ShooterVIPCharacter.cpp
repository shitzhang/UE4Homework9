// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/ShooterVIPCharacter.h"
#include "ShooterWeapon.h"
#include "ShooterPlayerState.h"
#include "prototype/prototype.h"

AShooterVIPCharacter::AShooterVIPCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	///* Ignore this channel or it will absorb the trace impacts instead of the skeletal mesh */
	//GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	//GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f, false);
	//GetCapsuleComponent()->SetCapsuleRadius(42.0f);

	///* These values are matched up to the CapsuleComponent above and are used to find navigation paths */
	//GetMovementComponent()->NavAgentProps.AgentRadius = 42;
	//GetMovementComponent()->NavAgentProps.AgentHeight = 192;

	Health = 100;
}

void AShooterVIPCharacter::BeginPlay()
{
	Super::BeginPlay();

	/* Assign a basic name to identify the bots in the HUD. */
	AShooterPlayerState* PS = Cast<AShooterPlayerState>(GetPlayerState());
	if (PS)
	{
		PS->SetPlayerName("VIPBot");
		PS->SetIsABot(true);
	}

	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AShooterWeapon* NewWeapon = GetWorld()->SpawnActor<AShooterWeapon>(
			DefaultWeaponClass, SpawnInfo);

		AddWeapon(NewWeapon);
	}
}