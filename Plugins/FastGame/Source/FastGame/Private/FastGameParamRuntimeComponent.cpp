#include "FastGameParamRuntimeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"

void UFastGameParamRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveTargets();
}

void UFastGameParamRuntimeComponent::ResolveTargets()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (!TargetSkeletalMesh)
	{
		TargetSkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (!TargetMesh)
	{
		TargetMesh = Owner->FindComponentByClass<UMeshComponent>();
	}
}

bool UFastGameParamRuntimeComponent::SetAnimatorFloat(FName ParamName, float Value)
{
	FFastGameBPParamWrite W;
	W.Name = ParamName;
	W.Channel = EFastGameParamChannel::Animator;
	W.Type = EFastGameParamValueType::Float;
	W.FloatValue = Value;
	return ApplyWrite(W);
}

bool UFastGameParamRuntimeComponent::SetAnimatorBool(FName ParamName, bool bValue)
{
	FFastGameBPParamWrite W;
	W.Name = ParamName;
	W.Channel = EFastGameParamChannel::Animator;
	W.Type = EFastGameParamValueType::Bool;
	W.bBoolValue = bValue;
	return ApplyWrite(W);
}

bool UFastGameParamRuntimeComponent::SetAnimatorTrigger(FName ParamName)
{
	FFastGameBPParamWrite W;
	W.Name = ParamName;
	W.Channel = EFastGameParamChannel::Animator;
	W.Type = EFastGameParamValueType::Trigger;
	return ApplyWrite(W);
}

bool UFastGameParamRuntimeComponent::SetMaterialScalar(FName ParamName, float Value)
{
	FFastGameBPParamWrite W;
	W.Name = ParamName;
	W.Channel = EFastGameParamChannel::Material;
	W.Type = EFastGameParamValueType::Float;
	W.FloatValue = Value;
	return ApplyWrite(W);
}

bool UFastGameParamRuntimeComponent::ApplyWrite(const FFastGameBPParamWrite& Write)
{
	ResolveTargets();
	if (Write.Name.IsNone())
	{
		OnParamFailed.Broadcast(TEXT("empty param name"));
		return false;
	}

	if (Write.Channel == EFastGameParamChannel::Animator)
	{
		if (!TargetSkeletalMesh)
		{
			OnParamFailed.Broadcast(TEXT("no SkeletalMesh for animator param"));
			return false;
		}
		UAnimInstance* Anim = TargetSkeletalMesh->GetAnimInstance();
		if (!Anim)
		{
			OnParamFailed.Broadcast(TEXT("no AnimInstance"));
			return false;
		}
		switch (Write.Type)
		{
		case EFastGameParamValueType::Bool:
			Anim->SetBool(Write.Name, Write.bBoolValue);
			break;
		case EFastGameParamValueType::Int:
			Anim->SetInteger(Write.Name, Write.IntValue);
			break;
		case EFastGameParamValueType::Float:
			Anim->SetFloat(Write.Name, Write.FloatValue);
			break;
		case EFastGameParamValueType::Trigger:
			Anim->SetTrigger(Write.Name);
			break;
		default:
			OnParamFailed.Broadcast(TEXT("unsupported animator type"));
			return false;
		}
		OnParamWritten.Broadcast(Write.Name, EFastGameParamChannel::Animator);
		return true;
	}

	if (Write.Channel == EFastGameParamChannel::Material)
	{
		if (!TargetMesh)
		{
			OnParamFailed.Broadcast(TEXT("no Mesh for material param"));
			return false;
		}
		const float Scalar = (Write.Type == EFastGameParamValueType::Bool)
			? (Write.bBoolValue ? 1.f : 0.f)
			: (Write.Type == EFastGameParamValueType::Int ? static_cast<float>(Write.IntValue) : Write.FloatValue);
		TargetMesh->SetScalarParameterValueOnMaterials(Write.Name, Scalar);
		OnParamWritten.Broadcast(Write.Name, EFastGameParamChannel::Material);
		return true;
	}

	// Component / FlowVar — reserved for Director / Flow host
	OnParamWritten.Broadcast(Write.Name, Write.Channel);
	return true;
}

void UFastGameParamRuntimeComponent::ApplyWrites(const TArray<FFastGameBPParamWrite>& Writes)
{
	for (const FFastGameBPParamWrite& W : Writes)
	{
		ApplyWrite(W);
	}
}
