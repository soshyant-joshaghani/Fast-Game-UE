#pragma once

#include "CoreMinimal.h"

/** Myket / Cafe Bazaar RSA is a public verify key. FG1 wrap is obfuscation, not encryption. */
FASTGAME_API bool FastGameNeedsStoreRsa(const FString& Provider);

/**
 * Decode GET /apps/games/catalog/{game}/store-verify-key rsa_verify_key.
 * Bound to game_code + provider. Returns false if wrap is invalid.
 */
FASTGAME_API bool FastGameUnwrapStoreVerifyKey(
	const FString& Wrapped,
	const FString& GameCode,
	const FString& Provider,
	FString& OutPem);
