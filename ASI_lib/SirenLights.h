#pragma once
#include <map>
#include <cstdint>
#include "RageStructs.h"



extern std::map<uint32_t, int32_t *> SirenBoneMap;
extern CSirenSettingsExpanded expandedSettings;
extern "C" CSirenSettingsExpanded * ExpandSirenSettings(CSirenSettings * src);
extern "C" void MakeBoneArrays(CVehicle * vehicle);
extern "C" int32_t CheckSirenBoneArray(uint32_t bone, CVehicle * vehicle);
extern "C" int32_t CheckGlassBoneArray(uint32_t bone, CVehicle * vehicle);
bool ApplySirenBufferHooks();

extern "C" uintptr_t DSL_RotateBeatTimeSet_ret;
extern "C" uintptr_t DSL_RotateBeatTimeSub_ret;
extern "C" uintptr_t DSL_PreCompute_ret;
extern "C" uintptr_t free_logic;
extern "C" uintptr_t DSL_BoneCheck_ret;
extern "C" uintptr_t DSL_Return_ret;
extern "C" uintptr_t CheckBroken_ret;
extern "C" uintptr_t CheckBrokenTwo_ret;
extern "C" uintptr_t CheckBrokenGlass_ret;
extern "C" uintptr_t CheckBrokenTwoGlass_ret;
extern "C" uintptr_t InitThingy_ret;
extern "C" void* DSL_PreCompute_patch;
extern "C" void* DSL_BoneCheck_patch;
extern "C" void* DSL_Return_patch;

extern "C" void* DSL_RotateBeatTimeSet_patch_pre3788;
extern "C" void* DSL_RotateBeatTimeSet_patch_post3788;

extern "C" void* DSL_RotateBeatTimeSub_patch_pre3788;
extern "C" void* DSL_RotateBeatTimeSub_patch_post3788;

extern "C" void* CheckBroken_patch_pre3788;
extern "C" void* CheckBroken_patch_post3788;

extern "C" void* CheckBrokenTwo_patch_pre3788;
extern "C" void* CheckBrokenTwo_patch_post3788;

extern "C" void* CheckBrokenGlass_patch_pre3788;
extern "C" void* CheckBrokenGlass_patch_post3788;

extern "C" void* CheckBrokenTwoGlass_patch_pre3788;
extern "C" void* CheckBrokenTwoGlass_patch_post3788;

extern "C" void* InitThingy_patch_pre3788;
extern "C" void* InitThingy_patch_post3788;