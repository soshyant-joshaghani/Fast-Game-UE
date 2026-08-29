#include "FastGameDownloadSceneComponent.h"
#include "FastGameSubsystem.h"
#include "FastGameClient.h"
#include "FastGamePackSelector.h"
#include "FastGamePackDownload.h"
#include "FastGameRuntimePlatform.h"
#include "FastGameLocalePrefs.h"
#include "FastGameSceneNames.h"
#include "FastGameTypes.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
	struct FFastGameDownloadState : TSharedFromThis<FFastGameDownloadState>
	{
		TWeakObjectPtr<UFastGameDownloadSceneComponent> Component;
		TSharedPtr<FFastGameClient> Client;
		FString GameCode;
		TArray<FFastGameAssetPack> Downloadable;
		int32 ResolveIndex = 0;
		int32 DownloadIndex = 0;

		void ResolveNext()
		{
			UFastGameDownloadSceneComponent* Self = Component.Get();
			if (!Self || !Client.IsValid())
			{
				return;
			}
			if (ResolveIndex >= Downloadable.Num())
			{
				if (Downloadable.Num() == 0)
				{
					Self->SetProgress(1.f, TEXT("No downloadable packs"));
					Self->FinishDownload();
					return;
				}
				DownloadIndex = 0;
				DownloadNext();
				return;
			}

			FFastGameAssetPack& Pack = Downloadable[ResolveIndex];
			if (FFastGamePackDownload::HasDownloadUrl(Pack))
			{
				++ResolveIndex;
				ResolveNext();
				return;
			}

			TSharedPtr<FFastGameDownloadState> State = AsShared();
			FFastGamePackDownload::ResolveDownloadUrl(Client->Content, GameCode, Pack,
				[State](bool bOk, const FString& /*Url*/, FString /*Error*/)
				{
					AsyncTask(ENamedThreads::GameThread, [State, bOk]()
					{
						if (!bOk)
						{
							State->Downloadable.RemoveAt(State->ResolveIndex);
						}
						else
						{
							++State->ResolveIndex;
						}
						State->ResolveNext();
					});
				});
		}

		void DownloadNext()
		{
			UFastGameDownloadSceneComponent* Self = Component.Get();
			if (!Self)
			{
				return;
			}
			if (DownloadIndex >= Downloadable.Num())
			{
				Self->SetProgress(1.f, TEXT("Complete"));
				Self->FinishDownload();
				return;
			}

			const FFastGameAssetPack& Pack = Downloadable[DownloadIndex];
			const FString Label = Pack.Label.IsEmpty() ? Pack.PackId : Pack.Label;
			Self->SetProgress(static_cast<float>(DownloadIndex) / Downloadable.Num(),
				FString::Printf(TEXT("Downloading %s…"), *Label));

			TSharedPtr<FFastGameDownloadState> State = AsShared();
			const FString Url = Pack.Url;
			FFastGamePackDownload::DownloadToCache(Pack, Url,
				[State, Label](bool /*bOk*/, FString /*Error*/)
				{
					AsyncTask(ENamedThreads::GameThread, [State, Label]()
					{
						if (UFastGameDownloadSceneComponent* Self = State->Component.Get())
						{
							Self->SetProgress(static_cast<float>(State->DownloadIndex + 1) / State->Downloadable.Num(),
								FString::Printf(TEXT("Downloaded %s"), *Label));
						}
						++State->DownloadIndex;
						State->DownloadNext();
					});
				});
		}
	};
}

UFastGameDownloadSceneComponent::UFastGameDownloadSceneComponent()
{
	SceneName = FName(FastGameSceneNames::Download);
	NextScene = FName(FastGameSceneNames::Menu);
}

void UFastGameDownloadSceneComponent::BeginPlay()
{
	Super::BeginPlay();
	StartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bAutoStart)
	{
		RunDownload();
	}
}

void UFastGameDownloadSceneComponent::RunDownload()
{
	SetProgress(0.f, TEXT("Preparing…"));

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFastGameSubsystem* Subsystem = GI ? GI->GetSubsystem<UFastGameSubsystem>() : nullptr;
	TSharedPtr<FFastGameClient> Client = Subsystem ? Subsystem->GetClient() : nullptr;
	if (!Subsystem || !Client.IsValid())
	{
		if (bAdvanceWhenNothingToDownload)
		{
			FinishDownload();
		}
		else
		{
			SetProgress(0.f, TEXT("Client not ready"));
		}
		return;
	}

	const FString GameCode = Subsystem->GetGameCode();
	TWeakObjectPtr<UFastGameDownloadSceneComponent> WeakThis(this);
	Client->Content->GetGameConfig(GameCode,
		[WeakThis, GameCode, Client, Subsystem](bool bOk, TSharedPtr<FJsonObject> Json, FString Error)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, GameCode, Client, Subsystem, bOk, Json, Error]()
			{
				UFastGameDownloadSceneComponent* Self = WeakThis.Get();
				if (!Self)
				{
					return;
				}
				if (!bOk)
				{
					if (Self->IsTipNotPublished(Error))
					{
						UE_LOG(LogTemp, Warning, TEXT("FastGame download: tip not published — publish tip in panel."));
						Self->SetProgress(0.f, TEXT("Tip not published — Publish tip in panel"));
					}
					else
					{
						Self->SetProgress(0.f, TEXT("Download failed"));
					}
					if (Self->bAdvanceWhenNothingToDownload)
					{
						Self->FinishDownload();
					}
					return;
				}

				const TArray<FFastGameAssetPack> AllPacks = FFastGameAssets::ListPacksFromGameTip(Json);
				FFastGameDownloadContext Context;
				Context.PreferredLanguage = FFastGameLocalePrefs::Get(TEXT("en"));
				Context.RuntimeOs = FFastGameRuntimePlatform::GetRuntimeOs();
				const FString StoreOs = FFastGameRuntimePlatform::StorePlatformToOs(Subsystem->GetStorePlatformId());
#if WITH_EDITOR
				if (!StoreOs.IsEmpty() && !Context.RuntimeOs.Equals(StoreOs, ESearchCase::IgnoreCase))
				{
					Context.RuntimeOs = StoreOs;
				}
#endif
				Context.QualityClass = FFastGameRuntimePlatform::GetQualityClass(Context.RuntimeOs);
				Context.bSkipSplashPacks = Self->bSkipSplashPacks;

				TArray<FFastGameAssetPack> Packs = FFastGamePackSelector::ListForDownload(AllPacks, Context);
				if (Packs.Num() == 0)
				{
					Self->SetProgress(1.f, TEXT("No packs for this device / language"));
					Self->FinishDownload();
					return;
				}

				TSharedRef<FFastGameDownloadState> State = MakeShared<FFastGameDownloadState>();
				State->Component = WeakThis;
				State->Client = Client;
				State->GameCode = GameCode;
				State->Downloadable = MoveTemp(Packs);
				State->ResolveNext();
			});
		});
}

bool UFastGameDownloadSceneComponent::IsTipNotPublished(const FString& Error) const
{
	return Error.Contains(TEXT("404")) || Error.Contains(TEXT("Tip not published"), ESearchCase::IgnoreCase);
}

void UFastGameDownloadSceneComponent::SetProgress(float Normalized, const FString& Message)
{
	OnDownloadProgress.Broadcast(FMath::Clamp(Normalized, 0.f, 1.f), Message);
}

void UFastGameDownloadSceneComponent::FinishDownload()
{
	if (bCompleted)
	{
		return;
	}
	const float Elapsed = GetWorld() ? (GetWorld()->GetTimeSeconds() - StartedAt) : 0.f;
	const float Wait = MinDisplaySeconds - Elapsed;
	if (Wait > 0.f && GetWorld())
	{
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bCompleted = true;
				CompleteScene();
			}),
			Wait,
			false);
		return;
	}
	bCompleted = true;
	CompleteScene();
}
