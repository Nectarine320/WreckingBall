#include "Player/Surface/WB_SurfaceMovementProfile.h"

const UWB_SurfaceMovementProfile* UWB_SurfaceMovementTable::FindProfile(TEnumAsByte<EPhysicalSurface> Surface) const
{
	for (const UWB_SurfaceMovementProfile* Profile : Surfaces)
	{
		if (Profile && Profile->SurfaceType == Surface)
		{
			return Profile;
		}
	}
	return DefaultProfile.Get();
}
