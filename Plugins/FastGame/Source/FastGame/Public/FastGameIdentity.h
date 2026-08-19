#pragma once

#include "CoreMinimal.h"
#include "Misc/Char.h"
#include "FastGameBlueprintTypes.h"

enum class EFastGameIdentityKind : uint8
{
	Unknown = 0,
	Email = 1,
	Phone = 2,
};

/**
 * Classify email vs phone for login / signup / recovery.
 * Login uses one backend endpoint (POST /base/login/access-token) with OAuth username;
 * the server resolves via find_user_by_identity (email or phone).
 */
namespace FastGameIdentity
{
	/** Normalize Iranian mobiles to 9xxxxxxxxx (matches backend normalize_iran_mobile). */
	inline bool TryNormalizePhone(const FString& Identity, FString& OutNormalized)
	{
		OutNormalized.Reset();
		FString Digits;
		Digits.Reserve(Identity.Len());
		for (const TCHAR Ch : Identity)
		{
			if (FChar::IsDigit(Ch))
			{
				Digits.AppendChar(Ch);
			}
		}
		if (Digits.StartsWith(TEXT("98")) && Digits.Len() >= 12)
		{
			Digits = Digits.Mid(2);
		}
		if (Digits.StartsWith(TEXT("0")) && Digits.Len() == 11)
		{
			Digits = Digits.Mid(1);
		}
		if (!Digits.StartsWith(TEXT("9")) || Digits.Len() != 10)
		{
			return false;
		}
		OutNormalized = Digits;
		return true;
	}

	inline EFastGameIdentityKind Classify(const FString& Identity)
	{
		const FString Trimmed = Identity.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return EFastGameIdentityKind::Unknown;
		}
		if (Trimmed.Contains(TEXT("@")))
		{
			int32 AtIndex = INDEX_NONE;
			if (!Trimmed.FindChar(TEXT('@'), AtIndex) || AtIndex <= 0 || AtIndex >= Trimmed.Len() - 1)
			{
				return EFastGameIdentityKind::Unknown;
			}
			const FString Domain = Trimmed.Mid(AtIndex + 1);
			if (!Domain.Contains(TEXT(".")) || Domain.StartsWith(TEXT(".")) || Domain.EndsWith(TEXT(".")))
			{
				return EFastGameIdentityKind::Unknown;
			}
			return EFastGameIdentityKind::Email;
		}
		FString Normalized;
		return TryNormalizePhone(Trimmed, Normalized) ? EFastGameIdentityKind::Phone : EFastGameIdentityKind::Unknown;
	}

	inline bool LooksLikeEmail(const FString& Identity)
	{
		return Classify(Identity) == EFastGameIdentityKind::Email;
	}

	inline bool LooksLikePhone(const FString& Identity)
	{
		return Classify(Identity) == EFastGameIdentityKind::Phone;
	}

	/** Split a single identity into exactly one of OutEmail / OutPhone (phone normalized). */
	inline bool TrySplitContact(const FString& Identity, FString& OutEmail, FString& OutPhone)
	{
		OutEmail.Reset();
		OutPhone.Reset();
		const FString Trimmed = Identity.TrimStartAndEnd();
		switch (Classify(Trimmed))
		{
		case EFastGameIdentityKind::Email:
			OutEmail = Trimmed.ToLower();
			return true;
		case EFastGameIdentityKind::Phone:
			return TryNormalizePhone(Trimmed, OutPhone);
		default:
			return false;
		}
	}

	/**
	 * Resolve Identity with channel mode (ENTER / Login contract).
	 * Auto → detect email vs phone. Email / Phone → force that channel.
	 */
	inline bool ResolveChannel(
		const FString& Identity,
		EFastGameIdentityChannel Channel,
		FString& OutEmail,
		FString& OutPhone,
		FString& OutError)
	{
		OutEmail.Reset();
		OutPhone.Reset();
		OutError.Reset();
		const FString Trimmed = Identity.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			OutError = TEXT("Identity is required");
			return false;
		}
		if (Channel == EFastGameIdentityChannel::Email)
		{
			if (!LooksLikeEmail(Trimmed))
			{
				OutError = TEXT("Valid email is required");
				return false;
			}
			OutEmail = Trimmed.ToLower();
			return true;
		}
		if (Channel == EFastGameIdentityChannel::Phone)
		{
			if (!TryNormalizePhone(Trimmed, OutPhone))
			{
				OutError = TEXT("Valid phone number is required");
				return false;
			}
			return true;
		}
		// Auto
		if (!TrySplitContact(Trimmed, OutEmail, OutPhone))
		{
			OutError = TEXT("Identity must be a valid email or phone number");
			return false;
		}
		return true;
	}

	/** Validate password + confirmation. Returns false and sets OutError on failure. */
	inline bool RequireMatchingPasswords(const FString& Password, const FString& PasswordConfirm, FString& OutError)
	{
		if (Password.IsEmpty())
		{
			OutError = TEXT("Password is required");
			return false;
		}
		if (Password.Len() < 8)
		{
			OutError = TEXT("Password must be at least 8 characters");
			return false;
		}
		if (Password != PasswordConfirm)
		{
			OutError = TEXT("Passwords do not match");
			return false;
		}
		OutError.Reset();
		return true;
	}
}
