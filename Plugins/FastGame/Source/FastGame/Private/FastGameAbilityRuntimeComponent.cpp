#include "FastGameAbilityRuntimeComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace FastGameAbilityUtil
{
	static EFastGameParamChannel ParseChannel(const FString& Raw)
	{
		const FString L = Raw.ToLower();
		if (L == TEXT("material"))
		{
			return EFastGameParamChannel::Material;
		}
		if (L == TEXT("component"))
		{
			return EFastGameParamChannel::Component;
		}
		if (L == TEXT("flow_var") || L == TEXT("flowvar"))
		{
			return EFastGameParamChannel::FlowVar;
		}
		return EFastGameParamChannel::Animator;
	}

	static EFastGameParamValueType ParseType(const FString& Raw)
	{
		const FString L = Raw.ToLower();
		if (L == TEXT("bool"))
		{
			return EFastGameParamValueType::Bool;
		}
		if (L == TEXT("int"))
		{
			return EFastGameParamValueType::Int;
		}
		if (L == TEXT("string"))
		{
			return EFastGameParamValueType::String;
		}
		if (L == TEXT("trigger"))
		{
			return EFastGameParamValueType::Trigger;
		}
		return EFastGameParamValueType::Float;
	}

	static void ParseWrites(const TArray<TSharedPtr<FJsonValue>>* Arr, TArray<FFastGameBPParamWrite>& Out)
	{
		Out.Reset();
		if (!Arr)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
			if (!V.IsValid() || !V->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
			FString Name;
			if (!Obj->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
			{
				continue;
			}
			FFastGameBPParamWrite W;
			W.Name = FName(*Name);
			FString Channel;
			Obj->TryGetStringField(TEXT("channel"), Channel);
			W.Channel = ParseChannel(Channel);
			FString Type;
			Obj->TryGetStringField(TEXT("type"), Type);
			W.Type = ParseType(Type);
			bool bBool = false;
			double Num = 0.0;
			FString Str;
			if (Obj->TryGetBoolField(TEXT("value"), bBool))
			{
				W.bBoolValue = bBool;
			}
			else if (Obj->TryGetNumberField(TEXT("value"), Num))
			{
				W.FloatValue = static_cast<float>(Num);
				W.IntValue = static_cast<int32>(Num);
			}
			else if (Obj->TryGetStringField(TEXT("value"), Str))
			{
				W.StringValue = Str;
			}
			Out.Add(W);
		}
	}

	static bool IsBuiltin(FName Id)
	{
		const FString S = Id.ToString().ToLower();
		return S == TEXT("move") || S == TEXT("jump") || S == TEXT("crouch") || S == TEXT("sprint");
	}
}

void UFastGameAbilityRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Params && GetOwner())
	{
		Params = GetOwner()->FindComponentByClass<UFastGameParamRuntimeComponent>();
	}
}

bool UFastGameAbilityRuntimeComponent::IsAbilityActive(FName AbilityId) const
{
	return ActiveAbilities.Contains(AbilityId);
}

const FFastGameBPAbilityDef* UFastGameAbilityRuntimeComponent::FindDef(FName AbilityId) const
{
	for (const FFastGameBPAbilityDef& Def : Abilities)
	{
		if (Def.AbilityId == AbilityId)
		{
			return &Def;
		}
	}
	return nullptr;
}

void UFastGameAbilityRuntimeComponent::LoadAbilitiesFromTipJson(const FString& JsonBody)
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
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Payload->TryGetArrayField(TEXT("abilities"), Arr) || !Arr)
	{
		return;
	}
	Abilities.Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!V.IsValid() || !V->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;
		FString AbilityId;
		if (!Obj->TryGetStringField(TEXT("ability_id"), AbilityId) || AbilityId.IsEmpty())
		{
			continue;
		}
		FFastGameBPAbilityDef Def;
		Def.AbilityId = FName(*AbilityId);
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
		if (Obj->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr && ParamsPtr->IsValid())
		{
			P = *ParamsPtr;
		}
		bool bToggle = false;
		P->TryGetBoolField(TEXT("toggle"), bToggle);
		Def.bToggle = bToggle;
		const TArray<TSharedPtr<FJsonValue>>* OnAct = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* OnDeact = nullptr;
		P->TryGetArrayField(TEXT("on_activate"), OnAct);
		P->TryGetArrayField(TEXT("on_deactivate"), OnDeact);
		FastGameAbilityUtil::ParseWrites(OnAct, Def.OnActivate);
		FastGameAbilityUtil::ParseWrites(OnDeact, Def.OnDeactivate);
		Abilities.Add(Def);
	}
}

void UFastGameAbilityRuntimeComponent::ActivateAbility(FName AbilityId)
{
	if (AbilityId.IsNone())
	{
		OnAbilityFailed.Broadcast(TEXT("empty ability id"));
		return;
	}
	const FFastGameBPAbilityDef* Def = FindDef(AbilityId);
	if (!Def)
	{
		if (FastGameAbilityUtil::IsBuiltin(AbilityId))
		{
			ActiveAbilities.Add(AbilityId);
			OnAbilityActivated.Broadcast(AbilityId);
			return;
		}
		OnAbilityFailed.Broadcast(FString::Printf(TEXT("unknown ability %s"), *AbilityId.ToString()));
		return;
	}
	if (Def->bToggle && ActiveAbilities.Contains(AbilityId))
	{
		DeactivateAbility(AbilityId);
		return;
	}
	if (Params)
	{
		Params->ApplyWrites(Def->OnActivate);
	}
	ActiveAbilities.Add(AbilityId);
	OnAbilityActivated.Broadcast(AbilityId);
}

void UFastGameAbilityRuntimeComponent::DeactivateAbility(FName AbilityId)
{
	if (!ActiveAbilities.Remove(AbilityId))
	{
		return;
	}
	if (const FFastGameBPAbilityDef* Def = FindDef(AbilityId))
	{
		if (Params)
		{
			Params->ApplyWrites(Def->OnDeactivate);
		}
	}
	OnAbilityDeactivated.Broadcast(AbilityId);
}

