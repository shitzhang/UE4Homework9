// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPowerupActor.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AShooterPowerupActor::AShooterPowerupActor()
{
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SphereComp->SetSphereRadius(75.0f);
	RootComponent = SphereComp;

	PowerupInterval = 0.0f;
	TotalNrOfTicks = 0;
	TicksProcessed = 0;

	bIsPowerupActive = false;

	SetReplicates(true);
}


void AShooterPowerupActor::OnTickPowerup()
{
	TicksProcessed++;

	OnPowerupTicked();

	if (TicksProcessed >= TotalNrOfTicks)
	{
		
		OnExpired();

		bIsPowerupActive = false;
		OnRep_PowerupActive();

		// Delete timer
		GetWorldTimerManager().ClearTimer(TimerHandle_PowerupTick);
	}
}

void AShooterPowerupActor::OnRep_PowerupActive()
{
	OnPowerupStateChanged(bIsPowerupActive);
}

void AShooterPowerupActor::ActivatePowerup(AActor* ActiveFor)
{
	OnActivated(ActiveFor);

	bIsPowerupActive = true;
	OnRep_PowerupActive();

	if (PowerupInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_PowerupTick, this, &AShooterPowerupActor::OnTickPowerup, PowerupInterval, true);
	}
	else
	{
		OnTickPowerup();
	}
}

void AShooterPowerupActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerState* PS = nullptr;
	if(Pawn)
	{
		PS = Pawn->GetPlayerState();
	}

	if(PS && !PS->IsABot())
	{
		if (HasAuthority())
		{
			ActivatePowerup(OtherActor);
		}
	}
}


void AShooterPowerupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPowerupActor, bIsPowerupActive);
}

