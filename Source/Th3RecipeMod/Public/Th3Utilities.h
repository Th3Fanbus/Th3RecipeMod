/* SPDX-License-Identifier: MPL-2.0 */

#pragma once

#include <CoreMinimal.h>

DECLARE_LOG_CATEGORY_EXTERN(LogTh3Utilities, Log, All);

/* Adapted from Templates/Invoke.h */
#define TH3_PROJECTION_CAPTURES(Captures, FuncName) \
	[Captures](auto&&... Args) -> decltype(auto) \
	{ \
		return FuncName(Forward<decltype(Args)>(Args)...); \
	}

#define TH3_PROJECTION_THIS(FuncName) \
	[this](auto&&... Args) -> decltype(auto) \
	{ \
		return FuncName(Forward<decltype(Args)>(Args)...); \
	}

#define TH3_LAMBDA_MEMBER(Obj, FuncName) \
	[Obj](auto&&... Args) -> decltype(auto) \
	{ \
		FuncName(Forward<decltype(Args)>(Args)...); \
	}

template <typename ElementType>
using Th3Optional = TArray<ElementType, TFixedAllocator<1>>;

namespace Th3Utilities
{
	template <size_t First, size_t Last>
	struct StaticFor
	{
		template <typename Fn>
		FORCEINLINE void operator()(Fn const& fn) const
		{
			if (First < Last) {
				fn(First);
				StaticFor<First + 1, Last>()(fn);
			}
		}
	};

	template <size_t N>
	struct StaticFor<N, N>
	{
		template <typename Fn>
		FORCEINLINE void operator()(Fn const& fn) const
		{
		}
	};

	template<typename T, size_t n>
	constexpr size_t array_size(const T(&)[n])
	{
		return n;
	}

	constexpr bool Implies(const bool& bAntecedent, const bool& bConsequent)
	{
		return not (bAntecedent and not bConsequent);
	}

	template <typename T>
	static FORCEINLINE void* Memset(void* Dest, T Value, size_t Count)
	{
		for (size_t i = 0; i < Count / sizeof(T); i++) {
			reinterpret_cast<T*>(Dest)[i] = Value;
		}
		return Dest;
	}

	/**
	 * More algos, derived from Algo::Transform in Unreal Engine.
	 */

	/**
	 * Applies a range-returning transform to a range
	 * and stores the results into a single container.
	 *
	 * @param  Input   Any iterable type
	 * @param  Output  Container to hold the output
	 * @param  Trans   Transformation operation
	 */

	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void TransformFlat(const InT& Input, OutT&& Output, TransformT Trans)
	{
		for (const auto& Value : Input) {
			Output.Append(Invoke(Trans, Value));
		}
	}

	/**
	 * Applies a series of transforms to a range
	 * and stores the results into a single container.
	 *
	 * @param  Input      Any iterable type
	 * @param  Output     Container to hold the output
	 * @param  IterTrans  Iterable with transformation operations
	 */
	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void TransformMulti(const InT& Input, OutT&& Output, const TransformT& IterTrans)
	{
		for (const auto& Value : Input) {
			for (const auto& Trans : IterTrans) {
				Output.Add(Invoke(Trans, Value));
			}
		}
	}

	/**
	 * Applies a series of range-returning transforms to a range
	 * and stores the results into a single container.
	 *
	 * @param  Input      Any iterable type
	 * @param  Output     Container to hold the output
	 * @param  IterTrans  Iterable with transformation operations
	 */
	template <typename InT, typename OutT, typename TransformT>
	FORCEINLINE void TransformMultiFlat(const InT& Input, OutT&& Output, const TransformT& IterTrans)
	{
		for (const auto& Value : Input) {
			for (const auto& Trans : IterTrans) {
				Output.Append(Invoke(Trans, Value));
			}
		}
	}

	/**
	 * Conditionally applies a series of transforms to a range
	 * and stores the results into a container
	 *
	 * @param  Input        Any iterable type
	 * @param  Output       Container to hold the output
	 * @param  TransMap     A `TArray<TPair<Predicate, Transform>>`, if `Predicate` is true for an element then `Transform` is applied to it
	 * @param  SingleMatch  If true, only apply the first transform for which its predicate is true, else apply all matching transforms
	 */
	template <typename InT, typename OutT, typename PredicateT, typename TransformT>
	FORCEINLINE void TransformIfMulti(const InT& Input, OutT&& Output, const TArray<TPair<PredicateT, TransformT>>& TransMap, const bool SingleMatch = true)
	{
		for (const auto& Value : Input) {
			for (const TPair<PredicateT, TransformT>& TransPair : TransMap) {
				if (Invoke(TransPair.Key, Value)) {
					Output.Add(Invoke(TransPair.Value, Value));
					if (SingleMatch) {
						break;
					}
				}
			}
		}
	}

	/**
	 * Applies class specific transforms to a range
	 * and stores the results into a single container.
	 *
	 * @param  Input     Any iterable type
	 * @param  Output    Container to hold the output
	 * @param  TransMap  Map of class to transformation operations
	 */
	template <typename InT, typename OutT, typename ClassT, typename TransformT>
	FORCEINLINE void TransformDynDispatch(const InT& Input, OutT&& Output, const TMap<ClassT, TransformT>& TransMap)
	{
		for (const auto& Value : Input) {
			for (const TPair<ClassT, TransformT>& TransPair : TransMap) {
				if (Value->GetClass()->IsChildOf(TransPair.Key)) {
					Output.Append(Invoke(TransPair.Value, Value));
					break;
				}
			}
		}
	}

	/**
	 * Invokes a class specific callable to each element in a range
	 *
	 * @param  Input    Any iterable type
	 * @param  CallMap  Map of class to callable
	 */
	template <typename InT, typename ClassT, typename CallableT>
	FORCEINLINE void ForEachDynDispatch(const InT& Input, const TMap<ClassT, CallableT>& CallMap)
	{
		for (const auto& Value : Input) {
			if (not Value) {
				continue;
			}
			const UClass* ValueClass = Value->GetClass();
			if (not ValueClass) {
				continue;
			}
			for (const TPair<ClassT, CallableT>& CallP: CallMap) {
				if (ValueClass->IsChildOf(CallP.Key)) {
					Invoke(CallP.Value, Value);
					break;
				}
			}
		}
	}

	/**
	 * Applies a transform to a range and then invokes a callable.
	 *
	 * @param  Input      Any iterable type
	 * @param  Trans      Transformation operation
	 * @param  Callable   Callable object
	 */
	template <typename InT, typename TransT, typename CallableT>
	FORCEINLINE void TransformForEach(InT& Input, TransT Transformer, CallableT Callable)
	{
		for (auto& Value : Input) {
			Invoke(Callable, Invoke(Transformer, Value));
		}
	}

	/**
	 * Applies a transform to a range and conditionally invokes a callable.
	 *
	 * @param  Input      Any iterable type
	 * @param  Trans      Transformation operation
	 * @param  Predicate  Condition which returns true for elements that should be called with and false for elements that should be skipped
	 * @param  Callable   Callable object
	 */
	template <typename InT, typename TransT, typename PredicateT, typename CallableT>
	FORCEINLINE void TransformForEachIf(InT& Input, TransT Transformer, PredicateT Predicate, CallableT Callable)
	{
		for (auto& Value : Input) {
			auto Result = Invoke(Transformer, Value);
			if (Invoke(Predicate, Result)) {
				Invoke(Callable, Result);
			}
		}
	}

	template <typename T>
	FORCEINLINE TSubclassOf<T> LoadTopLevelPathSync(const FTopLevelAssetPath& Path)
	{
		return TSoftClassPtr<T>(FSoftObjectPath(Path)).LoadSynchronous();
	}

	UClass* GenerateNewClass(const FString& Package, const FString& Name, UClass* ParentClass);
	void DumpObjectProperties(const UObject* Obj, const FString& Indent, FString& Data);
	void SaveObjectProperties(const UObject* Obj, const FString& FolderName, FString& Data);
	FORCEINLINE void SaveObjectProperties(const UObject* Obj, const FString& FolderName)
	{
		FString Data;
		SaveObjectProperties(Obj, FolderName, Data);
	}
	void DuplicateObjectProperties(UObject* OrigObj, UObject* NewObj);
	FORCEINLINE void DuplicateClassDefaults(UClass* OrigClass, UClass* NewClass)
	{
		DuplicateObjectProperties(OrigClass->GetDefaultObject(), NewClass->GetDefaultObject());
	}
	void DiscoverSubclassesOf(TSet<FTopLevelAssetPath>& out_AllClasses, UClass* BaseClass);

	/* Trying to copy Blueprint classes with an UberGraphFrame attribute fails miserably */
	template<typename T> TSubclassOf<T> AvoidClassUberGraphFrame(const TSubclassOf<T>& OrigClass)
	{
		const FName UberGraphFrame = FName(TEXT("UberGraphFrame"));
		for (TSubclassOf<T> SuperClass = OrigClass; SuperClass; SuperClass = SuperClass->GetSuperClass()) {
			if (SuperClass == T::StaticClass()) {
				return SuperClass;
			}
			if (SuperClass->FindPropertyByName(UberGraphFrame) == nullptr) {
				return SuperClass;
			}
		}
		UE_LOG(LogTh3Utilities, Error, TEXT("%s is not a subclass of %s"), *OrigClass->GetFullName(), *T::StaticClass()->GetFullName());
		return T::StaticClass();
	}

	template<typename T> TSubclassOf<T> CopyClassTo(const TSubclassOf<T>& OrigClass, const FString& PackageName, const FString& ClassName)
	{
		UE_LOG(LogTh3Utilities, Verbose, TEXT("[CopyClassTo] Generating new class %s %s"), *PackageName, *ClassName);
		TSubclassOf<T> NewClass = GenerateNewClass(PackageName, ClassName, AvoidClassUberGraphFrame(OrigClass));
		if (not NewClass) {
			UE_LOG(LogTh3Utilities, Error, TEXT("Failed to copy class into %s"), *ClassName);
			return nullptr;
		}
		DuplicateClassDefaults(OrigClass, NewClass);
		return NewClass;
	}

	template<typename T> TSubclassOf<T> CopyClassWithPrefix(const TSubclassOf<T> OrigClass, const FString& PackagePrefix, const FString& NamePrefix)
	{
		FString DumpData;
		DumpObjectProperties(OrigClass.GetDefaultObject(), TEXT(""), DumpData);
		UE_LOG(LogTh3Utilities, VeryVerbose, TEXT("\n%s"), *DumpData);
		UE_LOG(LogTh3Utilities, VeryVerbose, TEXT("Class Name    : %s"), *((UClass*)OrigClass)->GetName());
		UE_LOG(LogTh3Utilities, VeryVerbose, TEXT("Class PathName: %s"), *((UClass*)OrigClass)->GetPathName());
		const FString PackageName = PackagePrefix / OrigClass->GetPackage()->GetName();
		const FString ClassName = NamePrefix + OrigClass->GetName();
		UE_LOG(LogTh3Utilities, VeryVerbose, TEXT("Copying to: %s %s"), *PackageName, *ClassName);
		return CopyClassTo(OrigClass, PackageName, ClassName);
	}
};
