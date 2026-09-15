#include "FastGameCharacterControllerComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "FastGameAbilityRuntimeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UFastGameCharacterControllerComponent::UFastGameCharacterControllerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFastGameCharacterControllerComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveRefs();
	JumpsLeft = JumpCount;
	if (UCharacterMovementComponent* MovementComp = GetMovement())
	{
		MovementComp->JumpZVelocity = JumpZVelocity;
		MovementComp->MaxWalkSpeed = WalkSpeed;
	}
}

void UFastGameCharacterControllerComponent::ResolveRefs()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (!Params)
	{
		Params = Owner->FindComponentByClass<UFastGameParamRuntimeComponent>();
	}
	if (!Abilities)
	{
		Abilities = Owner->FindComponentByClass<UFastGameAbilityRuntimeComponent>();
	}
}

UCharacterMovementComponent* UFastGameCharacterControllerComponent::GetMovement() const
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		return Char->GetCharacterMovement();
	}
	return nullptr;
}

void UFastGameCharacterControllerComponent::ApplyMovementProfile(FName Profile)
{
	MovementProfile = Profile.IsNone() ? FName(TEXT("humanoid")) : Profile;
}

void UFastGameCharacterControllerComponent::ApplyLocomotionFromTipJson(const FString& JsonBody)
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
	FString MovementProfileStr;
	if (Payload->TryGetStringField(TEXT("movement_profile"), MovementProfileStr))
	{
		ApplyMovementProfile(FName(*MovementProfileStr));
	}
	double Walk = WalkSpeed;
	if (Payload->TryGetNumberField(TEXT("walk_speed"), Walk))
	{
		// Tip uses meters; CharacterMovement uses cm
		WalkSpeed = static_cast<float>(Walk) * (Walk > 20.0 ? 1.f : 100.f);
	}
	double Sprint = SprintMultiplier;
	if (Payload->TryGetNumberField(TEXT("sprint_mult"), Sprint))
	{
		SprintMultiplier = static_cast<float>(Sprint);
	}
	double CrouchM = CrouchMultiplier;
	if (Payload->TryGetNumberField(TEXT("crouch_mult"), CrouchM))
	{
		CrouchMultiplier = static_cast<float>(CrouchM);
	}
	double Jv = JumpZVelocity;
	if (Payload->TryGetNumberField(TEXT("jump_velocity"), Jv))
	{
		JumpZVelocity = static_cast<float>(Jv) * (Jv > 50.0 ? 1.f : 100.f);
	}
	double Jc = JumpCount;
	if (Payload->TryGetNumberField(TEXT("jump_count"), Jc))
	{
		JumpCount = FMath::Max(1, static_cast<int32>(Jc));
		JumpsLeft = JumpCount;
	}
	const TSharedPtr<FJsonObject>* StatsPtr = nullptr;
	if (Payload->TryGetObjectField(TEXT("stats"), StatsPtr) && StatsPtr && StatsPtr->IsValid())
	{
		const TSharedPtr<FJsonObject>& Stats = *StatsPtr;
		if (Stats->TryGetNumberField(TEXT("walk_speed"), Walk))
		{
			WalkSpeed = static_cast<float>(Walk) * (Walk > 20.0 ? 1.f : 100.f);
		}
		if (Stats->TryGetNumberField(TEXT("jump_count"), Jc))
		{
			JumpCount = FMath::Max(1, static_cast<int32>(Jc));
			JumpsLeft = JumpCount;
		}
		if (Stats->TryGetNumberField(TEXT("jump_velocity"), Jv))
		{
			JumpZVelocity = static_cast<float>(Jv) * (Jv > 50.0 ? 1.f : 100.f);
		}
	}
	if (UCharacterMovementComponent* MovementComp = GetMovement())
	{
		MovementComp->JumpZVelocity = JumpZVelocity;
		MovementComp->MaxWalkSpeed = WalkSpeed;
	}
}

void UFastGameCharacterControllerComponent::SetSprint(bool bInSprint)
{
	bSprint = bInSprint;
	if (Params && !SprintParamName.IsNone())
	{
		Params->SetAnimatorBool(SprintParamName, bSprint);
	}
}

void UFastGameCharacterControllerComponent::SetCrouch(bool bInCrouch)
{
	bCrouch = bInCrouch;
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (bCrouch)
		{
			Char->Crouch();
		}
		else
		{
			Char->UnCrouch();
		}
	}
	if (Params && !CrouchParamName.IsNone())
	{
		Params->SetAnimatorBool(CrouchParamName, bCrouch);
	}
}

void UFastGameCharacterControllerComponent::RequestJump()
{
	if (JumpsLeft <= 0)
	{
		return;
	}
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		Char->Jump();
		JumpsLeft--;
		if (Abilities)
		{
			Abilities->ActivateAbility(TEXT("jump"));
		}
	}
}

void UFastGameCharacterControllerComponent::TickComponent(
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
		return;
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

	if (PC->WasInputKeyJustPressed(EKeys::LeftControl) || PC->WasInputKeyJustPressed(EKeys::C))
	{
		SetCrouch(!bCrouch);
		if (Abilities)
		{
			if (bCrouch)
			{
				Abilities->ActivateAbility(TEXT("crouch"));
			}
			else
			{
				Abilities->DeactivateAbility(TEXT("crouch"));
			}
		}
	}

	const bool bWantSprint =
		(PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)) && !bCrouch;
	if (bWantSprint != bSprint)
	{
		SetSprint(bWantSprint);
		if (Abilities)
		{
			if (bSprint)
			{
				Abilities->ActivateAbility(TEXT("sprint"));
			}
			else
			{
				Abilities->DeactivateAbility(TEXT("sprint"));
			}
		}
	}

	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		RequestJump();
	}

	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (Char->GetCharacterMovement() && Char->GetCharacterMovement()->IsMovingOnGround())
		{
			JumpsLeft = JumpCount;
		}
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

	float Scale = 1.f;
	if (bCrouch)
	{
		Scale *= CrouchMultiplier;
	}
	else if (bSprint)
	{
		Scale *= SprintMultiplier;
	}

	if (UCharacterMovementComponent* MovementComp = GetMovement())
	{
		MovementComp->MaxWalkSpeed = WalkSpeed * Scale;
	}

	Pawn->AddMovementInput(Pawn->GetActorForwardVector(), Input.Y);
	Pawn->AddMovementInput(Pawn->GetActorRightVector(), Input.X);

	ResolveRefs();
	if (Params && !SpeedParamName.IsNone())
	{
		Params->SetAnimatorFloat(SpeedParamName, Input.Size() * Scale);
	}
}
