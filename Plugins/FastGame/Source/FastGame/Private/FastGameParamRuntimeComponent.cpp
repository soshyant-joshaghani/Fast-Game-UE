#include "FastGameParamRuntimeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

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

namespace FastGameParamRuntimeUtil
{
	static bool WriteAnimInstanceProperty(UAnimInstance* Anim, const FFastGameBPParamWrite& Write)
	{
		if (!Anim || Write.Name.IsNone())
		{
			return false;
		}
		// UE 5.6+: AnimBP variables are UObject properties (SetFloat/SetBool removed from UAnimInstance).
		FProperty* Prop = Anim->GetClass()->FindPropertyByName(Write.Name);
		if (!Prop)
		{
			return false;
		}
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Anim);
		switch (Write.Type)
		{
		case EFastGameParamValueType::Bool:
		case EFastGameParamValueType::Trigger:
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
			{
				BoolProp->SetPropertyValue(
					ValuePtr,
					Write.Type == EFastGameParamValueType::Trigger ? true : Write.bBoolValue);
				return true;
			}
			break;
		case EFastGameParamValueType::Int:
			if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
			{
				IntProp->SetPropertyValue(ValuePtr, Write.IntValue);
				return true;
			}
			break;
		case EFastGameParamValueType::Float:
			if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				FloatProp->SetPropertyValue(ValuePtr, Write.FloatValue);
				return true;
			}
			if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
			{
				DoubleProp->SetPropertyValue(ValuePtr, static_cast<double>(Write.FloatValue));
				return true;
			}
			break;
		default:
			break;
		}
		return false;
	}
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
		if (!FastGameParamRuntimeUtil::WriteAnimInstanceProperty(Anim, Write))
		{
			OnParamFailed.Broadcast(
				FString::Printf(TEXT("AnimBP has no writable property '%s'"), *Write.Name.ToString()));
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
