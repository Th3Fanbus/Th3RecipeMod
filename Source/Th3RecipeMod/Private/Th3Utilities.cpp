/* SPDX-License-Identifier: MPL-2.0 */

#include "Th3Utilities.h"

#include <Algo/AnyOf.h>
#include <Algo/NoneOf.h>
#include <Algo/Transform.h>
#include <AssetRegistry/AssetRegistryModule.h>
#include <Logging/LogMacros.h>
#include <Logging/StructuredLog.h>
#include <Reflection/ClassGenerator.h>

DEFINE_LOG_CATEGORY(LogTh3Utilities);

UClass* Th3Utilities::GenerateNewClass(const TCHAR* Package, FString Name, UClass* ParentClass)
{
	if (Name == "") {
		UE_LOG(LogTh3Utilities, Fatal, TEXT("Name was empty, can't create class"));
		return nullptr;
	}
	FTopLevelAssetPath Path = FTopLevelAssetPath(FString::Printf(TEXT("%s%s"), Package, *Name));
	if (FindObject<UClass>(Path)) {
		UE_LOG(LogTh3Utilities, Error, TEXT("Class for name %s already exists"), *Name);
		UE_LOG(LogTh3Utilities, Fatal, TEXT("Found %s"), *Path.ToString());
		return nullptr;
	}
	FTopLevelAssetPath Path_C = FTopLevelAssetPath(FString::Printf(TEXT("%s%s_C"), Package, *Name));
	if (FindObject<UClass>(Path_C)) {
		UE_LOG(LogTh3Utilities, Error, TEXT("Class for name %s already exists"), *Name);
		UE_LOG(LogTh3Utilities, Fatal, TEXT("Found %s"), *Path_C.ToString());
		return nullptr;
	}
	return FClassGenerator::GenerateSimpleClass(Package, *Name, ParentClass);
}

static FString PrintObjStrings(const FString& Indent, const FString& Descriptor, UObject* Obj)
{
	FString Data;
	if (Obj) {
		Data += FString::Printf(TEXT("%s%s name: %s\n"), *Indent, *Descriptor, *Obj->GetName());
		Data += FString::Printf(TEXT("%s%s path: %s\n"), *Indent, *Descriptor, *Obj->GetPathName());
		Data += FString::Printf(TEXT("%s%s full: %s\n"), *Indent, *Descriptor, *Obj->GetFullName());
	}
	return Data;
}

static FString PrintObjClassStrings(const FString& Indent, const FString& Descriptor, UObject* Obj)
{
	FString Data;
	if (Obj) {
		Data += PrintObjStrings(Indent, FString::Printf(TEXT("      %s"), *Descriptor), Obj);
		Data += PrintObjStrings(Indent, FString::Printf(TEXT("%s class"), *Descriptor), Obj->GetClass());
	}
	return Data;
}

void Th3Utilities::DumpObjectProperties(UObject* Obj, const FString& Indent, FString& Data)
{
	if (Obj) {
		Data += PrintObjStrings(Indent, TEXT(" Object"), Obj);
		Data += PrintObjStrings(Indent, TEXT("  Outer"), Obj->GetOuter());
		Data += PrintObjStrings(Indent, TEXT("Package"), Obj->GetPackage());
		UClass* Class = Obj->GetClass();
		while (Class) {
			Data += FString::Printf(TEXT("%s- Properties inherited from class '%s'\n"), *Indent, *Class->GetName());
			for (TFieldIterator<FProperty> Prop(Class, EFieldIteratorFlags::ExcludeSuper); Prop; ++Prop) {
				FString Value;
				Prop->ExportText_InContainer(0, Value, Obj, Obj, NULL, 0);
				Data += FString::Printf(TEXT("%s\t- %s = %s\n"), *Indent, *Prop->GetName(), *Value);
			}
			Class = Class->GetSuperClass();
		}
		Data += TEXT("\n\n");
	} else {
		Data += FString::Printf(TEXT("%sNULL\n"), *Indent);
	}
}

void Th3Utilities::SaveObjectProperties(UObject* Obj, const FString& FolderName, FString& Data)
{
	if (Obj) {
		DumpObjectProperties(Obj, TEXT(""), Data);
		const FString FileName = FString::Printf(TEXT("%s/%s/%s.txt"), *FolderName, *Obj->GetPathName(), *Obj->GetName());
		UE_LOG(LogTh3Utilities, Display, TEXT("Saving log data to %s"), *FileName);
		FString FilePath = FPaths::ProjectSavedDir() / FileName;
		FFileHelper::SaveStringToFile(Data, *FilePath);
	} else {
		UE_LOG(LogTh3Utilities, Error, TEXT("Got nullptr, not saving properties in %s"), *FolderName);
		UE_LOG(LogTh3Utilities, Verbose, TEXT("\n%s"), *Data);
	}
}

void Th3Utilities::DuplicateObjectProperties(UObject* OrigObj, UObject* NewObj)
{
	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
	CopyParams.bPreserveRootComponent = false;
	CopyParams.bPerformDuplication = true;
	CopyParams.bDoDelta = false;
	UEngine::CopyPropertiesForUnrelatedObjects(OrigObj, NewObj, CopyParams);
}

/* Special thanks to Archengius for this snippet of code */
void Th3Utilities::DiscoverSubclassesOf(TSet<FTopLevelAssetPath>& out_AllClasses, UClass* BaseClass)
{
	TArray<FTopLevelAssetPath> NativeRootClassPaths;
	TArray<UClass*> NativeRootClasses;
	GetDerivedClasses(BaseClass, NativeRootClasses);
	NativeRootClasses.Add(BaseClass);

	auto predicate = [](const UClass* RootClass) { return RootClass and RootClass->HasAnyClassFlags(CLASS_Native); };
	auto transform = &UClass::GetClassPathName;
	Algo::TransformIf(NativeRootClasses, NativeRootClassPaths, predicate, transform);

	IAssetRegistry::Get()->GetDerivedClassNames(NativeRootClassPaths, {}, out_AllClasses);
	out_AllClasses.Append(NativeRootClassPaths);
}