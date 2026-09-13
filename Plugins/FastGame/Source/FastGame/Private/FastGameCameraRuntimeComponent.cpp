#include "FastGameCameraRuntimeComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

UFastGameCameraRuntimeComponent::UFastGameCameraRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFastGameCameraRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!RigCamera)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AActor* View = PC->GetViewTarget())
			{
				RigCamera = View->FindComponentByClass<UCameraComponent>();
			}
		}
	}
}

void UFastGameCameraRuntimeComponent::ApplyCameraProfile(FName Profile)
{
	CameraProfile = Profile.IsNone() ? FName(TEXT("tps_follow")) : Profile;
}

void UFastGameCameraRuntimeComponent::SetFollowTarget(AActor* Target)
{
	FollowTarget = Target;
}

void UFastGameCameraRuntimeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!RigCamera || !FollowTarget)
	{
		return;
	}

	const FString Profile = CameraProfile.ToString().ToLower();
	if (Profile == TEXT("fixed"))
	{
		return;
	}

	FVector Desired;
	if (Profile == TEXT("fps"))
	{
		Desired = FollowTarget->GetActorLocation() + FollowTarget->GetActorForwardVector() * 10.f + FVector(0, 0, 160.f);
		const FRotator NewRot = FMath::RInterpTo(
			RigCamera->GetComponentRotation(),
			FollowTarget->GetActorRotation(),
			DeltaTime,
			FollowInterpSpeed);
		RigCamera->SetWorldLocationAndRotation(Desired, NewRot);
		return;
	}
	if (Profile == TEXT("top_down"))
	{
		Desired = FollowTarget->GetActorLocation() + TopDownOffset;
	}
	else
	{
		Desired = FollowTarget->GetActorLocation() + FollowTarget->GetActorRotation().RotateVector(TpsOffset);
	}

	const FVector NewLoc = FMath::VInterpTo(
		RigCamera->GetComponentLocation(),
		Desired,
		DeltaTime,
		FollowInterpSpeed);
	const FVector LookAt = FollowTarget->GetActorLocation() + FVector(0, 0, 140.f);
	const FRotator LookRot = (LookAt - NewLoc).Rotation();
	RigCamera->SetWorldLocationAndRotation(NewLoc, LookRot);
}
