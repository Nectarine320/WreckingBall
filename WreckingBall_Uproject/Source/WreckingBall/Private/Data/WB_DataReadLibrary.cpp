#include "Data/WB_DataReadLibrary.h"

#include "UObject/UnrealType.h"

bool UWB_DataReadLibrary::ReadNumber(const UObject* Object, FName Variable, float& OutValue)
{
	OutValue = 0.f;
	const FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Variable) : nullptr;
	if (!Property)
	{
		return false;
	}
	const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
	OutValue = Property->IsFloatingPoint()
		? static_cast<float>(Property->GetFloatingPointPropertyValue(Value))
		: static_cast<float>(Property->GetSignedIntPropertyValue(Value));
	return true;
}

UObject* UWB_DataReadLibrary::ReadObject(const UObject* Object, FName Variable)
{
	const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Variable) : nullptr;
	return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

bool UWB_DataReadLibrary::ReadGameplayTag(const UObject* Object, FName Variable, FGameplayTag& OutTag)
{
	OutTag = FGameplayTag();
	const FStructProperty* Property = Object ? FindFProperty<FStructProperty>(Object->GetClass(), Variable) : nullptr;
	if (!Property || Property->Struct != FGameplayTag::StaticStruct())
	{
		return false;
	}
	OutTag = *Property->ContainerPtrToValuePtr<FGameplayTag>(Object);
	return true;
}

UObject* UWB_DataReadLibrary::CallObjectGetter(UObject* Object, FName Function)
{
	UFunction* Func = Object ? Object->FindFunction(Function) : nullptr;
	if (!Func)
	{
		return nullptr;
	}
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(FMath::Max<int32>(Func->ParmsSize, 1)));
	FMemory::Memzero(Params, FMath::Max<int32>(Func->ParmsSize, 1));
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}
	Object->ProcessEvent(Func, Params);

	UObject* Result = nullptr;
	for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It);
		if (!Result && ObjectProperty && ObjectProperty->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
		{
			Result = ObjectProperty->GetObjectPropertyValue_InContainer(Params);
		}
		It->DestroyValue_InContainer(Params);
	}
	return Result;
}
