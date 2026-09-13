#include "FastGameAbilityRuntimeComponent.h"
#include "FastGameParamRuntimeComponent.h"
#include "GameFramework/Actor.h"

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
