/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>
#include <Math/Color.h>
#include <Algo/Accumulate.h>

struct FPreciseColor
{
	double R;
	double G;
	double B;
	double A;

	FPreciseColor() = default;
	FPreciseColor(const FPreciseColor& Color) = default;
	FPreciseColor(double R, double G, double B, double A = 1.0) : R(R), G(G), B(B), A(A)
	{
	}
	FPreciseColor(const FFloat16Color& Color) : R(Color.R), G(Color.G), B(Color.B), A(Color.A)
	{
	}
	FPreciseColor(const FLinearColor& Color) : R(Color.R), G(Color.G), B(Color.B), A(Color.A)
	{
	}
	FPreciseColor(const FColor Color) : R(Color.R / 255.0), G(Color.G / 255.0), B(Color.B / 255.0), A(Color.A / 255.0)
	{
	}
	FPreciseColor(const FDXTColor565 Color, uint8_t Alpha = 0xff) : FPreciseColor(FColor(Color.R << 3, Color.G << 2, Color.B << 3, Alpha))
	{
	}
	FPreciseColor(const FDXTColor16 Color, uint8_t Alpha = 0xff) : FPreciseColor(Color.Color565, Alpha)
	{
	}

	FORCEINLINE bool operator==(const FPreciseColor& Other) const
	{
		return this->R == Other.R and this->G == Other.G and this->B == Other.B and this->A == Other.A;
	}
	FORCEINLINE bool operator!=(const FPreciseColor& Other) const
	{
		return not operator==(Other);
	}

	FORCEINLINE FPreciseColor& operator+=(const FPreciseColor& Other)
	{
		R += Other.R;
		G += Other.G;
		B += Other.B;
		A += Other.A;
		return *this;
	}
	FORCEINLINE FPreciseColor operator+(const FPreciseColor& Other) const
	{
		return FPreciseColor(*this) += Other;
	}

	FORCEINLINE FPreciseColor& operator-=(const FPreciseColor& Other)
	{
		R -= Other.R;
		G -= Other.G;
		B -= Other.B;
		A -= Other.A;
		return *this;
	}
	FORCEINLINE FPreciseColor operator-(const FPreciseColor& Other) const
	{
		return FPreciseColor(*this) -= Other;
	}

	FORCEINLINE FPreciseColor& operator*=(const FPreciseColor& Other)
	{
		R *= Other.R;
		G *= Other.G;
		B *= Other.B;
		A *= Other.A;
		return *this;
	}
	FORCEINLINE FPreciseColor operator*(const FPreciseColor& Other) const
	{
		return FPreciseColor(*this) *= Other;
	}

	FORCEINLINE FPreciseColor& operator*=(const double Scalar)
	{
		R *= Scalar;
		G *= Scalar;
		B *= Scalar;
		A *= Scalar;
		return *this;
	}
	FORCEINLINE FPreciseColor operator*(const double Scalar) const
	{
		return FPreciseColor(*this) *= Scalar;
	}

	FORCEINLINE FPreciseColor& operator/=(const FPreciseColor& Other)
	{
		R /= Other.R;
		G /= Other.G;
		B /= Other.B;
		A /= Other.A;
		return *this;
	}
	FORCEINLINE FPreciseColor operator/(const FPreciseColor& Other) const
	{
		return FPreciseColor(*this) /= Other;
	}

	FORCEINLINE FPreciseColor& operator/=(const double Scalar)
	{
		const double InvScalar = 1.0f / Scalar;
		return operator*=(InvScalar);
	}
	FORCEINLINE FPreciseColor operator/(const double Scalar) const
	{
		return FPreciseColor(*this) /= Scalar;
	}

	FORCEINLINE FPreciseColor WithAlpha(double Alpha) const
	{
		return FPreciseColor(R, G, B, Alpha);
	}

	FORCEINLINE FColor ToFColor(const bool bSRGB) const
	{
		return FLinearColor(R, G, B, A).ToFColor(bSRGB);
	}

	FORCEINLINE FString ToString() const
	{
		return FString::Printf(TEXT("(R=%f, G=%f, B=%f, A=%f)"), R, G, B, A);
	}

	static FORCEINLINE FPreciseColor Average(const std::initializer_list<FPreciseColor> Colors)
	{
		return Algo::Accumulate(Colors, FPreciseColor()) / Colors.size();
	}

	static FORCEINLINE FPreciseColor Over(const FPreciseColor& Bot, const FPreciseColor& Top)
	{
		const double TopCoef = Top.A;
		const double BotCoef = Bot.A * (1.0 - Top.A);

		const double Alpha = TopCoef + BotCoef;

		if (FMath::IsNearlyZero(Alpha)) {
			return FPreciseColor();
		}
		return (Top * TopCoef + Bot * BotCoef) / Alpha;
	}
};