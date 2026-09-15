#include "FastGameCameraControllerComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UFastGameCameraControllerComponent::UFastGameCameraControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFastGameCameraControllerComponent::BeginPlay()
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

FName UFastGameCameraControllerComponent::NormalizeCameraProfile(FName Profile)
{
	FString P = Profile.ToString().ToLower().Replace(TEXT("-"), TEXT("_"));
	if (P == TEXT("tps_follow") || P == TEXT("third_person"))
	{
		P = TEXT("tps");
	}
	else if (P == TEXT("first_person"))
	{
		P = TEXT("fps");
	}
	else if (P == TEXT("topdown"))
	{
		P = TEXT("top_down");
	}
	else if (P == TEXT("side_scroll"))
	{
		P = TEXT("side");
	}
	return FName(*P);
}

void UFastGameCameraControllerComponent::ApplyCameraProfile(FName Profile)
{
	const FName Normalized = NormalizeCameraProfile(Profile.IsNone() ? FName(TEXT("tps")) : Profile);
	if (AllowedCameras.Num() > 0)
	{
		bool bOk = false;
		for (const FName& Allowed : AllowedCameras)
		{
			if (NormalizeCameraProfile(Allowed) == Normalized)
			{
				bOk = true;
				break;
			}
		}
		if (!bOk)
		{
			return;
		}
	}
	CameraProfile = Normalized;
}

void UFastGameCameraControllerComponent::SetFollowTarget(AActor* Target)
{
	FollowTarget = Target;
}

void UFastGameCameraControllerComponent::ApplyAllowedCamerasFromTipJson(const FString& JsonBody)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	TSharedPtr<FJsonObject> Payload = Root;
	if (Root->TryGetObjectField(TEXT("payload"), PayloadPtr) && PayloadPtr && PayloadPtr->IsValid())
	{
		Payload = *PayloadPtr;
	}
	const TArray<TSharedPtr<FJsonValue>>* Cams = nullptr;
	if (!Payload->TryGetArrayField(TEXT("cameras"), Cams) || !Cams)
	{
		const TSharedPtr<FJsonObject>* StatsPtr = nullptr;
		if (Payload->TryGetObjectField(TEXT("stats"), StatsPtr) && StatsPtr && StatsPtr->IsValid())
		{
			(*StatsPtr)->TryGetArrayField(TEXT("cameras"), Cams);
		}
	}
	if (!Cams)
	{
		return;
	}
	AllowedCameras.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Cams)
	{
		FString S;
		if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
		{
			AllowedCameras.Add(FName(*S));
		}
	}
}

void UFastGameCameraControllerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!RigCamera || !FollowTarget)
	{
		return;
	}

	const FString Profile = NormalizeCameraProfile(CameraProfile).ToString();
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
	else if (Profile == TEXT("side"))
	{
		Desired = FollowTarget->GetActorLocation() + FollowTarget->GetActorRotation().RotateVector(SideOffset);
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
