#include "FastGameMovementRuntimeComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UFastGameMovementRuntimeComponent::UFastGameMovementRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFastGameMovementRuntimeComponent::ApplyMovementProfile(FName Profile)
{
	MovementProfile = Profile.IsNone() ? FName(TEXT("humanoid")) : Profile;
}

void UFastGameMovementRuntimeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bEnablePlayerInput)
	{
		return;
	}

	const FString Profile = MovementProfile.ToString();
	if (Profile.StartsWith(TEXT("vehicle_"), ESearchCase::IgnoreCase))
	{
		return; // G4
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	const float Forward =
		(PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) +
		(PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down) ? -1.f : 0.f);
	const float Right =
		(PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) +
		(PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left) ? -1.f : 0.f);

	FVector2D Input(Right, Forward);
	if (Input.SizeSquared() > 1.f)
	{
		Input.Normalize();
	}

	const bool bSprint = PC->IsInputKeyDown(EKeys::LeftShift);
	const float Scale = WalkSpeedScale * (bSprint ? 1.6f : 1.f);
	Pawn->AddMovementInput(Pawn->GetActorForwardVector(), Input.Y * Scale);
	Pawn->AddMovementInput(Pawn->GetActorRightVector(), Input.X * Scale);

	if (UFastGameParamRuntimeComponent* Params = GetOwner()->FindComponentByClass<UFastGameParamRuntimeComponent>())
	{
		if (!SpeedParamName.IsNone())
		{
			Params->SetAnimatorFloat(SpeedParamName, Input.Size() * Scale);
		}
	}
}
