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
