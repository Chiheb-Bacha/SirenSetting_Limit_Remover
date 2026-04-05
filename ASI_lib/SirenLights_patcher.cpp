#include "pch.h"
#include "SirenSettings_patcher.h"
#include "SirenLights.h"
#include "RageStructs.h"
#include "hooking.h"
#include "debug.h"
#include "Utils.h"
#include <cstdlib>
#include <cassert>

CRITICAL_SECTION mapMutex;
static const bool post3788 = GetGameVersion() >= FileVersion(1, 0, 3788, 0);

std::map<uint32_t, int32_t*> SirenBoneMap;
std::map<uint32_t, int32_t*> GlassBoneMap;

int32_t(*GetBoneIndexFromId)(void* entity, uint16_t boneTag) = NULL;

const Pattern DrawSirenLights_Pattern = { "0f 29 70 a8 0f 29 78 98 44 0f 29 40 88 4c 8b f9 44 0f 29 88 78 ff ff ff 44 0f 29 90 68 ff ff ff 44 0f 29 98 58 ff ff ff", 0x27 };
const Pattern GetHeadlightStatus_Pattern = { "0f b6 d2 41 0f b6 c0 4c 8b c9 8d 0c 50 0f b6 d1 41 8b 89 84 01 00 00", 0 };
const Pattern GetHeadlightIntensity_Pattern = { "0f b6 d2 41 0f b6 c0 8d 14 50 f3 0f 10 84 91 6c 01 00 00 c3", 0 };
const Pattern CheckForBrokenSirens_Pattern = post3788 ?
Pattern{ "0f 2f 0d ?? ?? ?? ?? 0f 29 70 ?? 0f 29 78 ?? 49 8b f1", 0x23 } : 
Pattern{ "4c 8b 8f ?? ?? ?? ?? 49 8b 41 20 48 8b 88 b0 00 00 00 48 8b 01 0f be 54 30 10", 0x68e };
const Pattern CheckForBrokenSirensTwo_Pattern = post3788 ?
Pattern{ "80 7d ?? 00 0f 84 ?? ?? ?? ?? 48 8b 83 ?? ?? ?? ?? 48 8b 88", 0x4E } :
Pattern{ "41 8b cf 45 8b f7 41 bc 01 00 00 00 83 e1 1f 49 c1 ee 05 41 d3 e4", 0x412 };
const Pattern InitThingy_Pattern = post3788 ?
Pattern{ "48 8b ec 48 83 ec 60 48 8b 81 ?? ?? ?? ?? 33 ff 48 8b d9 f6 80", 0x14 } :
Pattern{ "41 8b c6 41 8b ce 48 c1 e8 05 83 e1 1f 8b 84 87", 0x68 };

int32_t CheckSirenBoneArray(uint32_t bone, CVehicle* vehicle)
{
	uint32_t vehName = vehicle->modelInfo->nameHash;
	assert(SirenBoneMap.count(vehName));
	return SirenBoneMap.at(vehName)[bone];
}

int32_t CheckGlassBoneArray(uint32_t bone, CVehicle* vehicle)
{
	uint32_t vehName = vehicle->modelInfo->nameHash;
	assert(GlassBoneMap.count(vehName));
	return GlassBoneMap.at(vehName)[bone];
}

void MakeBoneArrays(CVehicle* vehicle)
{
	uint32_t vehName = vehicle->modelInfo->nameHash;
	EnterCriticalSection(&mapMutex);
	logDebug("%8.8x: ", vehName);
	if (SirenBoneMap.count(vehName))
	{
		logDebug("FOUND %p\n", SirenBoneMap.at(vehName));
	}
	else
	{
		logDebug("Building... ");
		int32_t* newSirenArray = (int32_t*)malloc(NUM_LIGHTS * sizeof(int32_t));
		int32_t* newGlassArray = (int32_t*)malloc(NUM_LIGHTS * sizeof(int32_t));
		if (newSirenArray == NULL || newGlassArray == NULL)
		{
			LeaveCriticalSection(&mapMutex);
			return;
		}
		for (int i = 0; i < NUM_LIGHTS; i++)
		{
			newSirenArray[i] = GetBoneIndexFromId((void*)vehicle, SirenBoneTags[i]);
			newGlassArray[i] = GetBoneIndexFromId((void*)vehicle, GlassBoneTags[i]);
		}
		SirenBoneMap[vehName] = newSirenArray;
		GlassBoneMap[vehName] = newGlassArray;
		logDebug("Built: %p\n", newSirenArray);
	}
	LeaveCriticalSection(&mapMutex);
	return;
}

void InitializeSirenBufferBesidesSeed(SirenBuffer* buffer)
{
	buffer->BeatNumber = -1;
	buffer->AdjustedTime = 0;
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		buffer->FlasherLastBeatMask[i] = 0;
		buffer->FlasherLastBeatTime[i] = 0;
		buffer->RotatorLastBeatMask[i] = 0;
		buffer->RotatorLastBeatTime[i] = 0;
	}
	for (int i = 0; i < 4; i++)
	{
		buffer->HeadlightLastBeatMask[i] = 0;
		buffer->HeadlightLastBeatTime[i] = 0;
		buffer->HeadlightIntensity[i] = 1.0f;
	}
	buffer->RotatorStatus = 0;
	buffer->FlasherStatus = 0;
	buffer->HeadlightStatus = 0;
}

bool ApplyMiscBoneCheckHooksPost3788()
{
	uintptr_t CheckBrokenLoc = FindPattern(CheckForBrokenSirens_Pattern);
	if (CheckBrokenLoc == NULL)
	{
		return false;
	}

	logDebug("CheckBrokenLoc: %p\n", CheckBrokenLoc);
	CheckBroken_ret = InsertHookWithSkip(CheckBrokenLoc + 0x6C3, CheckBrokenLoc + 0x6DB, (uintptr_t)&CheckBroken_patch_post3788);
	if (CheckBroken_ret == NULL)
		return false;
	int32_t endBoneId = *(int32_t*)(CheckBrokenLoc + 0x6A3) + (NUM_LIGHTS_SUPPORTED - 20);
	WriteForeignMemory(CheckBrokenLoc + 0x6A3, &endBoneId, 4);
	uint8_t newCmp[7] = { 0x48, 0x81, 0xFB, (uint8_t)endBoneId, 0x00, 0x00, 0x00 };
	WriteForeignMemory(CheckBrokenLoc + 0x801, newCmp, 7);
	CheckBrokenGlass_ret = InsertNearHookWithSkip(CheckBrokenLoc + 0x79E, CheckBrokenLoc + 0x7A5, (uintptr_t)&CheckBrokenGlass_patch_post3788);
	if (CheckBrokenGlass_ret == NULL)
		return false;

	uintptr_t CheckBrokenLocTwo = FindPattern(CheckForBrokenSirensTwo_Pattern);
	if (CheckBrokenLocTwo == NULL)
		return false;
	logDebug("CheckBrokenLocTwo: %p\n", CheckBrokenLocTwo);
	CheckBrokenTwo_ret = InsertHookWithSkip(CheckBrokenLocTwo + 0x436, CheckBrokenLocTwo + 0x44E, (uintptr_t)&CheckBrokenTwo_patch_post3788);
	if (CheckBrokenTwo_ret == NULL)
		return false;
	WriteForeignMemory(CheckBrokenLocTwo + 0x5A1, &endBoneId, 4);
	WriteForeignMemory(CheckBrokenLocTwo + 0x3F9, &endBoneId, 4);
	CheckBrokenTwoGlass_ret = InsertNearHookWithSkip(CheckBrokenLocTwo + 0x542, CheckBrokenLocTwo + 0x549, (uintptr_t)&CheckBrokenTwoGlass_patch_post3788);
	if (CheckBrokenTwoGlass_ret == NULL)
		return false;

	uintptr_t InitThingyLoc = FindPattern(InitThingy_Pattern);
	if (InitThingyLoc == NULL)
		return false;
	logDebug("InitThingyLoc: %p\n", InitThingyLoc);
	InitThingy_ret = InsertHookWithSkip(InitThingyLoc + 0x96, InitThingyLoc + 0xAE, (uintptr_t)&InitThingy_patch_post3788);
	if (InitThingy_ret == NULL)
		return false;
	return WriteForeignMemory(InitThingyLoc + 0xCC, &endBoneId, 4);
}

bool ApplyMiscBoneCheckHooksPre3788()
{
	uintptr_t CheckBrokenLoc = FindPattern(CheckForBrokenSirens_Pattern);
	if (CheckBrokenLoc == NULL)
		return false;
	logDebug("CheckBrokenLoc: %p\n", CheckBrokenLoc);
	CheckBroken_ret = InsertHookWithSkip(CheckBrokenLoc + 0x695, CheckBrokenLoc + 0x6a8, (uintptr_t)&CheckBroken_patch_pre3788);
	if (CheckBroken_ret == NULL)
		return false;
	int32_t endBoneId = *(int32_t*)(CheckBrokenLoc + 0x7bc) + (NUM_LIGHTS_SUPPORTED - 20);
	WriteForeignMemory(CheckBrokenLoc + 0x7bc, &endBoneId, 4);
	CheckBrokenGlass_ret = InsertNearHookWithSkip(CheckBrokenLoc + 0x758, CheckBrokenLoc + 0x75d, (uintptr_t)&CheckBrokenGlass_patch_pre3788);
	if (CheckBrokenGlass_ret == NULL)
		return false;

	uintptr_t CheckBrokenLocTwo = FindPattern(CheckForBrokenSirensTwo_Pattern);
	if (CheckBrokenLocTwo == NULL)
		return false;
	logDebug("CheckBrokenLocTwo: %p\n", CheckBrokenLocTwo);
	CheckBrokenTwo_ret = InsertHookWithSkip(CheckBrokenLocTwo + 0x3f6, CheckBrokenLocTwo + 0x409, (uintptr_t)&CheckBrokenTwo_patch_pre3788);
	if (CheckBrokenTwo_ret == NULL)
		return false;
	WriteForeignMemory(CheckBrokenLocTwo + 0x55b, &endBoneId, 4);
	CheckBrokenTwoGlass_ret = InsertNearHookWithSkip(CheckBrokenLocTwo + 0x501, CheckBrokenLocTwo + 0x506, (uintptr_t)&CheckBrokenTwoGlass_patch_pre3788);
	if (CheckBrokenTwoGlass_ret == NULL)
		return false;

	uintptr_t InitThingyLoc = FindPattern(InitThingy_Pattern);
	if (InitThingyLoc == NULL)
		return false;
	logDebug("InitThingyLoc: %p\n", InitThingyLoc);
	InitThingy_ret = InsertHookWithSkip(InitThingyLoc + 0x88, InitThingyLoc + 0x9b, (uintptr_t)&InitThingy_patch_pre3788);
	if (InitThingy_ret == NULL)
		return false;
	return WriteForeignMemory(InitThingyLoc + 0xb2, &endBoneId, 4);
}

// Adding the version comparisons within 1 function would be cumbersome
// Hence why I separated the logic into two functions depending on the version
bool ApplyMiscBoneCheckHooks() {
	return post3788 ? ApplyMiscBoneCheckHooksPost3788() : ApplyMiscBoneCheckHooksPre3788();
}

bool ApplySirenBufferHooks()
{
	InitializeCriticalSection(&mapMutex);
	free_logic = (uintptr_t)&free;
	pattern BufferMallocPtn = { "48 8b d8 48 85 c0 74 0c 33 d2 48 8b c8", 0x5b5 };
	uintptr_t BufferMallocLoc = FindPattern(BufferMallocPtn);
	if (BufferMallocLoc == NULL)
	{
		return false;
	}
	logDebug("BufferMallocLoc: %p\n", BufferMallocLoc);
	uint32_t BufferSize = sizeof(SirenBuffer);
	WriteForeignMemory(BufferMallocLoc + 0x5ac, &BufferSize, sizeof(uint32_t));
	uintptr_t BufferInitLoc = GetReferencedAddress(BufferMallocLoc + 0x5c2);
	if (BufferInitLoc == NULL)
		return false;
	logDebug("BufferInitLoc: %p\n", BufferInitLoc);

	if (InsertHookWithSkip(BufferInitLoc + 0x1c, BufferInitLoc + 0x2d, (uintptr_t)&InitializeSirenBufferBesidesSeed) == NULL)
	{
		return false;
	}
	pattern BoneIndexPtn = { "e8 ?? ?? ?? ?? 8b d0 83 f8 ff 74 17 48 8b 07 4c 8d 44 24", 0x61 };
	uintptr_t BoneIndexLoc = FindPattern(BoneIndexPtn);
	if (BoneIndexLoc == NULL)
	{
		return false;
	}
	logDebug("BoneIndexLoc: %p\n", BoneIndexLoc);
	BoneIndexLoc = GetReferencedAddress(BoneIndexLoc + 0x61);
	if (BoneIndexLoc == NULL)
	{
		return false;
	}
	logDebug("BoneIndexLoc 2: %p\n", BoneIndexLoc);
	GetBoneIndexFromId = (int32_t(*)(void*, uint16_t))BoneIndexLoc;

	bool bonechecks = ApplyMiscBoneCheckHooks();
	if (!bonechecks)
		return false;

	uintptr_t DrawSirenLights = FindPattern(DrawSirenLights_Pattern);
	if (DrawSirenLights == NULL)
		return false;
	logDebug("DrawSirenLights: %p\n", DrawSirenLights);

	uintptr_t GetHeadlightStatus = FindPattern(GetHeadlightStatus_Pattern);
	uintptr_t GetHeadlightIntensity = FindPattern(GetHeadlightIntensity_Pattern);

	if (!GetHeadlightIntensity || !GetHeadlightStatus)
		return false;

	logDebug("GetHeadlightStatus: %p\n", GetHeadlightStatus);
	logDebug("GetHeadlightIntensity: %p\n", GetHeadlightIntensity);

	bool success = true;

	{
		uint32_t RotatorBeatMask = offsetof(SirenBuffer, RotatorLastBeatMask);
		uint32_t RotatorBeatTime = offsetof(SirenBuffer, RotatorLastBeatTime);
		uint32_t FlasherBeatMask = offsetof(SirenBuffer, FlasherLastBeatMask);
		uint32_t FlasherBeatTime = offsetof(SirenBuffer, FlasherLastBeatTime);
		uint32_t LightBeatMask = offsetof(SirenBuffer, HeadlightLastBeatMask);
		uint32_t LightBeatTime = offsetof(SirenBuffer, HeadlightLastBeatTime);
		uint32_t LightIntensity = offsetof(SirenBuffer, HeadlightIntensity);
		uint32_t LightStatus = offsetof(SirenBuffer, HeadlightStatus);
		uint32_t FlasherStatus = offsetof(SirenBuffer, FlasherStatus);
		uint32_t RotatorStatus = offsetof(SirenBuffer, RotatorStatus);
		uint8_t SirenCount = NUM_LIGHTS_SUPPORTED; // TODO: Make 0x28
		uint32_t MaxBoneId = 0;


		// Post-3788
		uintptr_t FlasherBeatMaskRefsPost3788[3] = { 0x5d5, 0x5ea, 0x5f8 };
		uintptr_t FlasherBeatTimeRefsPost3788[2] = { 0x607, 0x629 };
		uintptr_t FlasherStatusRefsPost3788[6] = { 0x5C0, 0x73C, 0x74A, 0x80D, 0x882, 0xA45 };
		uintptr_t RotatorStatusRefsPost3788[7] = { 0x56C, 0x4C6, 0x588, 0x757, 0xA3b, 0x386, 0x4B9 };
		uintptr_t LightOffsetRefsPost3788[1] = { 0x194 };
		uintptr_t LightStatusRefsPost3788[3] = { 0x1EE, 0x276, 0x26C };
		uintptr_t SirenCountRefsPost3788[4] = { 0x432, 0x48A, 0x709, 0x684 };
		// Pre-3788
		uintptr_t FlasherBeatMaskRefsPre3788[3] = { 0x5be, 0x5d3, 0x5dd };
		uintptr_t FlasherBeatTimeRefsPre3788[2] = { 0x5ec, 0x610 };
		uintptr_t FlasherStatusRefsPre3788[6] = { 0x5a9, 0x728, 0x736, 0x80f, 0x884, 0xa03 };
		uintptr_t RotatorStatusRefsPre3788[7] = { 0x378, 0x4ad, 0x4ba, 0x538, 0x578, 0x73f, 0x9f9 };
		uintptr_t LightOffsetRefsPre3788[1] = { 0x196 };
		uintptr_t LightStatusRefsPre3788[3] = { 0x1f0, 0x26e, 0x278 };
		uintptr_t SirenCountRefsPre3788[4] = { 0x424, 0x481, 0x66a, 0x6f5 };

		uintptr_t* FlasherBeatMaskRefs = post3788 ? FlasherBeatMaskRefsPost3788 : FlasherBeatMaskRefsPre3788;
		uintptr_t* FlasherBeatTimeRefs = post3788 ? FlasherBeatTimeRefsPost3788 : FlasherBeatTimeRefsPre3788;
		uintptr_t* FlasherStatusRefs = post3788 ? FlasherStatusRefsPost3788 : FlasherStatusRefsPre3788;
		uintptr_t* RotatorStatusRefs = post3788 ? RotatorStatusRefsPost3788 : RotatorStatusRefsPre3788;
		uintptr_t* LightOffsetRefs = post3788 ? LightOffsetRefsPost3788 : LightOffsetRefsPre3788;
		uintptr_t* LightStatusRefs = post3788 ? LightStatusRefsPost3788 : LightStatusRefsPre3788;
		uintptr_t* SirenCountRefs = post3788 ? SirenCountRefsPost3788 : SirenCountRefsPre3788;

		for (int i = 0; i < 3; i++)
			success = success && WriteForeignMemory(DrawSirenLights + FlasherBeatMaskRefs[i], &FlasherBeatMask, sizeof(uint32_t));
		for (int i = 0; i < 2; i++)
			success = success && WriteForeignMemory(DrawSirenLights + FlasherBeatTimeRefs[i], &FlasherBeatTime, sizeof(uint32_t));
		for (int i = 0; i < 6; i++)
			success = success && WriteForeignMemory(DrawSirenLights + FlasherStatusRefs[i], &FlasherStatus, sizeof(uint32_t));
		for (int i = 0; i < 7; i++)
			success = success && WriteForeignMemory(DrawSirenLights + RotatorStatusRefs[i], &RotatorStatus, sizeof(uint32_t));
		for (int i = 0; i < 1; i++)
			success = success && WriteForeignMemory(DrawSirenLights + LightOffsetRefs[i], &LightBeatMask, sizeof(uint32_t));
		for (int i = 0; i < 3; i++)
			success = success && WriteForeignMemory(DrawSirenLights + LightStatusRefs[i], &LightStatus, sizeof(uint32_t));
		for (int i = 0; i < 4; i++)
			success = success && WriteForeignMemory(DrawSirenLights + SirenCountRefs[i], &SirenCount, sizeof(uint8_t));

		if (post3788) {
			DSL_RotateBeatTimeSet_ret = InsertNearHookWithSkip(DrawSirenLights + 0x3BC, DrawSirenLights + 0x3C1, (uintptr_t)&DSL_RotateBeatTimeSet_patch_post3788);
			DSL_RotateBeatTimeSub_ret = InsertNearHookWithSkip(DrawSirenLights + 0x40D, DrawSirenLights + 0x411, (uintptr_t)&DSL_RotateBeatTimeSub_patch_post3788);
			DSL_BoneCheck_ret = InsertHookWithSkip(DrawSirenLights + 0x323, DrawSirenLights + 0x340, (uintptr_t)&DSL_BoneCheck_patch);
			DSL_PreCompute_ret = InsertNearHook(DrawSirenLights + 0x91, (uintptr_t)&DSL_PreCompute_patch);
			DSL_Return_ret = InsertNearHook(DrawSirenLights + 0x10D1, (uintptr_t)&DSL_Return_patch);
			success = success && DSL_RotateBeatTimeSet_ret && DSL_RotateBeatTimeSub_ret && DSL_BoneCheck_ret && DSL_PreCompute_ret && DSL_Return_ret;

			MaxBoneId = *(uint32_t*)(DrawSirenLights + 0xE87);
			MaxBoneId += (NUM_LIGHTS_SUPPORTED - 20); // TODO: Make 20
			success = success && WriteForeignMemory(DrawSirenLights + 0xE87, &MaxBoneId, sizeof(uint32_t));
		}
		else {
			DSL_RotateBeatTimeSet_ret = InsertNearHookWithSkip(DrawSirenLights + 0x3ae, DrawSirenLights + 0x3b3, (uintptr_t)&DSL_RotateBeatTimeSet_patch_pre3788);
			DSL_RotateBeatTimeSub_ret = InsertNearHookWithSkip(DrawSirenLights + 0x403, DrawSirenLights + 0x408, (uintptr_t)&DSL_RotateBeatTimeSub_patch_pre3788);
			DSL_BoneCheck_ret = InsertHookWithSkip(DrawSirenLights + 0x31b, DrawSirenLights + 0x332, (uintptr_t)&DSL_BoneCheck_patch);
			DSL_PreCompute_ret = InsertNearHook(DrawSirenLights + 0x91, (uintptr_t)&DSL_PreCompute_patch);
			DSL_Return_ret = InsertNearHook(DrawSirenLights + 0x108d, (uintptr_t)&DSL_Return_patch);
			success = success && DSL_RotateBeatTimeSet_ret && DSL_RotateBeatTimeSub_ret && DSL_BoneCheck_ret && DSL_PreCompute_ret && DSL_Return_ret;

			MaxBoneId = *(uint32_t*)(DrawSirenLights + 0xe43);
			MaxBoneId += (NUM_LIGHTS_SUPPORTED - 20); // TODO: Make 20
			success = success && WriteForeignMemory(DrawSirenLights + 0xe43, &MaxBoneId, sizeof(uint32_t));
		}
	}
	if (!success)
		return false;
	uint32_t LightIntensity = offsetof(SirenBuffer, HeadlightIntensity);
	uint32_t LightStatus = offsetof(SirenBuffer, HeadlightStatus);
	if (!WriteForeignMemory(GetHeadlightIntensity + 0xf, &LightIntensity, sizeof(uint32_t)))
		return false;
	if (!WriteForeignMemory(GetHeadlightStatus + 0x13, &LightStatus, sizeof(uint32_t)))
		return false;

	return true;
}

CSirenSettingsExpanded* ExpandSirenSettings(CSirenSettings* src)
{
	CSirenSettingsExpanded* dst = (CSirenSettingsExpanded*)malloc(sizeof(CSirenSettingsExpanded));
	if (dst == NULL)
		return NULL;
	memcpy(dst, src, 0x3c);
	dst->Name = src->Name;
	dst->NumSirens = src->NumSirens;
	int16_t srcLightNum = src->lightSize;
	if (srcLightNum > NUM_LIGHTS)
		srcLightNum = NUM_LIGHTS;
	memcpy(dst->Lights, src->Lights, srcLightNum * sizeof(EmergencyLight));
	memset(dst->Lights + srcLightNum, NULL, (NUM_LIGHTS - srcLightNum) * sizeof(EmergencyLight));
	return dst;
}