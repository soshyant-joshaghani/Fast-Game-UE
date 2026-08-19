#include "FastGameStoreVerify.h"
#include "FastGameTypes.h"
#include "Misc/Base64.h"

// Compact SHA-256 (FIPS 180-4). Used so FG1 unwrap matches the backend on any UE version.
namespace FastGameSha256
{
	static uint32 Rotr(uint32 X, uint32 N)
	{
		return (X >> N) | (X << (32 - N));
	}

	static void Transform(uint32 H[8], const uint8 Block[64])
	{
		static const uint32 K[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
		};
		uint32 W[64];
		for (int32 i = 0; i < 16; ++i)
		{
			W[i] = (uint32(Block[i * 4]) << 24) | (uint32(Block[i * 4 + 1]) << 16)
				| (uint32(Block[i * 4 + 2]) << 8) | uint32(Block[i * 4 + 3]);
		}
		for (int32 i = 16; i < 64; ++i)
		{
			const uint32 S0 = Rotr(W[i - 15], 7) ^ Rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
			const uint32 S1 = Rotr(W[i - 2], 17) ^ Rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
			W[i] = W[i - 16] + S0 + W[i - 7] + S1;
		}
		uint32 a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
		for (int32 i = 0; i < 64; ++i)
		{
			const uint32 S1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
			const uint32 Ch = (e & f) ^ ((~e) & g);
			const uint32 T1 = h + S1 + Ch + K[i] + W[i];
			const uint32 S0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
			const uint32 Maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32 T2 = S0 + Maj;
			h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
		}
		H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
	}

	static void Hash(const uint8* Data, int32 Len, uint8 Out[32])
	{
		uint32 H[8] = {
			0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
		};
		uint8 Block[64];
		int32 Offset = 0;
		while (Len - Offset >= 64)
		{
			FMemory::Memcpy(Block, Data + Offset, 64);
			Transform(H, Block);
			Offset += 64;
		}
		const int32 Rem = Len - Offset;
		FMemory::Memzero(Block, 64);
		if (Rem > 0)
		{
			FMemory::Memcpy(Block, Data + Offset, Rem);
		}
		Block[Rem] = 0x80;
		const uint64 BitLen = uint64(Len) * 8ull;
		if (Rem >= 56)
		{
			Transform(H, Block);
			FMemory::Memzero(Block, 64);
		}
		for (int32 i = 0; i < 8; ++i)
		{
			Block[63 - i] = static_cast<uint8>((BitLen >> (8 * i)) & 0xff);
		}
		Transform(H, Block);
		for (int32 i = 0; i < 8; ++i)
		{
			Out[i * 4] = static_cast<uint8>((H[i] >> 24) & 0xff);
			Out[i * 4 + 1] = static_cast<uint8>((H[i] >> 16) & 0xff);
			Out[i * 4 + 2] = static_cast<uint8>((H[i] >> 8) & 0xff);
			Out[i * 4 + 3] = static_cast<uint8>(H[i] & 0xff);
		}
	}
}

namespace
{
	FString BytesToUtf8String(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() <= 0)
		{
			return FString();
		}
		TArray<ANSICHAR> Ansi;
		Ansi.SetNumUninitialized(Bytes.Num() + 1);
		FMemory::Memcpy(Ansi.GetData(), Bytes.GetData(), Bytes.Num());
		Ansi[Bytes.Num()] = 0;
		return UTF8_TO_TCHAR(Ansi.GetData());
	}
}

bool FastGameNeedsStoreRsa(const FString& Provider)
{
	const FString Id = FastGameNormalizeProviderId(Provider);
	return Id.Equals(TEXT("myket"), ESearchCase::IgnoreCase)
		|| Id.Equals(TEXT("caffebazar"), ESearchCase::IgnoreCase);
}

bool FastGameUnwrapStoreVerifyKey(
	const FString& Wrapped,
	const FString& GameCode,
	const FString& Provider,
	FString& OutPem)
{
	OutPem.Empty();
	const FString Blob = Wrapped.TrimStartAndEnd();
	const FString Prefix = TEXT("FG1.");
	if (!Blob.StartsWith(Prefix))
	{
		return false;
	}
	FString Token = Blob.Mid(Prefix.Len());
	Token.ReplaceInline(TEXT("-"), TEXT("+"));
	Token.ReplaceInline(TEXT("_"), TEXT("/"));
	while (Token.Len() % 4 != 0)
	{
		Token += TEXT("=");
	}
	TArray<uint8> Xored;
	if (!FBase64::Decode(Token, Xored) || Xored.Num() <= 0)
	{
		return false;
	}
	const FString Material = FString(TEXT("fastgame.store-verify.v1"))
		+ TEXT("|")
		+ GameCode.TrimStartAndEnd()
		+ TEXT("|")
		+ FastGameNormalizeProviderId(Provider);
	const FTCHARToUTF8 Utf8(*Material);
	uint8 Mask[32];
	FastGameSha256::Hash(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Mask);
	TArray<uint8> PemBytes;
	PemBytes.SetNumUninitialized(Xored.Num());
	for (int32 i = 0; i < Xored.Num(); ++i)
	{
		PemBytes[i] = static_cast<uint8>(Xored[i] ^ Mask[i % 32]);
	}
	OutPem = BytesToUtf8String(PemBytes).TrimStartAndEnd();
	return !OutPem.IsEmpty();
}
