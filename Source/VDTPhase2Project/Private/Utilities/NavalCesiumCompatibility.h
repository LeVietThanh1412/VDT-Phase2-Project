#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
// expected-lite's no-exception SEH path imports windows.h. Initialize the SDK
// through Unreal before Cesium so min/max and GetCurrentTime do not leak.
#include "Windows/WindowsHWrapper.h"
// GDI's macro is not removed by Unreal's wrapper and collides with glTF's
// Material::AlphaMode::OPAQUE. This module does not use the Win32 GDI macro.
#ifdef OPAQUE
#undef OPAQUE
#endif
#endif
