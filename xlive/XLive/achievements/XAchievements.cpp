#include "stdafx.h"
#include "XAchievements.h"

#include "H2MOD/Modules/Achievements/Achievements.h"

/* globals */

static DWORD achievementCount = 0;
static DWORD achievementEnumeratorFlags = 0;
static DWORD achievementEnumeratorIndex = 0;

HANDLE g_dwFakeAchievementContent = INVALID_HANDLE_VALUE;

/* public code */

// #5278: XUserWriteAchievements
DWORD WINAPI XUserWriteAchievements(
	DWORD dwNumAchievements,
	PXUSER_ACHIEVEMENT pAchievement,
	PXOVERLAPPED pOverlapped)
{
	// LAN edition: online achievements removed - accept the write, do nothing
	LOG_TRACE_XLIVE(
		"XUserWriteAchievements  (count = {0:#x}, buffer = {1:p}, pOverlapped = {2:p}) - ignored",
		dwNumAchievements,
		(void*)pAchievement,
		(void*)pOverlapped);

	if (pOverlapped)
	{
		pOverlapped->InternalLow = ERROR_SUCCESS;
		pOverlapped->InternalHigh = dwNumAchievements;
		pOverlapped->dwExtendedError = 0;

		return ERROR_IO_PENDING;
	}

	return ERROR_SUCCESS;
}

// #5280: XUserCreateAchievementEnumerator
DWORD WINAPI XUserCreateAchievementEnumerator(
	DWORD dwTitleId,
	DWORD dwUserIndex,
	XUID xuid,
	DWORD dwDetailFlags,
	DWORD dwStartingIndex, 
	DWORD cItem,
	PDWORD pchBuffer,
	PHANDLE phEnum)
{
	LOG_TRACE_XLIVE("XUserCreateAchievementEnumerator (dwStartingIndex = {0}, MaxEnumerator = {1})", dwStartingIndex, cItem);

	achievementCount = cItem;
	achievementEnumeratorFlags = dwDetailFlags;
	achievementEnumeratorIndex = dwStartingIndex;

	if (pchBuffer) *pchBuffer = cItem * sizeof(XACHIEVEMENT_DETAILS);
	if (phEnum) *phEnum = g_dwFakeAchievementContent = CreateMutex(NULL, NULL, NULL);

	LOG_TRACE_XLIVE("- Handle = {}, pchBuffer = {}", (void*)g_dwFakeAchievementContent, pchBuffer ? *pchBuffer : 0);

	return ERROR_SUCCESS;
}

int AchievementEnumerator(
	DWORD cbBuffer, 
	CHAR* pvBuffer,
	PDWORD pcItemsReturned,
	XOVERLAPPED* pOverlapped)
{
	memset(pOverlapped, 0, sizeof(XOVERLAPPED));

	for (; achievementEnumeratorIndex < achievementCount; achievementEnumeratorIndex++)
	{
		XACHIEVEMENT_DETAILS aaa;
		memset(&aaa, 0, sizeof(XACHIEVEMENT_DETAILS));
		bool achieved = g_achievement_list[achievementEnumeratorIndex + 1];

		// check max
		if ((achievementCount - (achievementEnumeratorIndex + 1)) >= cbBuffer / sizeof(XACHIEVEMENT_DETAILS))
			break;

		FILETIME fileTime;
		SYSTEMTIME systemTime;

		GetSystemTime(&systemTime);
		SystemTimeToFileTime(&systemTime, &fileTime);

		aaa.dwId = achievementEnumeratorIndex + 1;
		aaa.pwszLabel = L"";
		aaa.pwszDescription = L"";
		aaa.pwszUnachieved = L"";
		aaa.dwImageId = 0;

		if (achieved)
		{
			aaa.dwCred = 1000;
			aaa.ftAchieved = fileTime;
		}

		aaa.dwFlags = achieved == true ? XACHIEVEMENT_DETAILS_ACHIEVED_ONLINE | XACHIEVEMENT_DETAILS_ACHIEVED : 0;

		if (pOverlapped == NULL)
		{
			(*pcItemsReturned)++;
		}
		else
		{
			pOverlapped->InternalHigh++;
		}

		if (pvBuffer)
		{
			memcpy(pvBuffer, &aaa, sizeof(XACHIEVEMENT_DETAILS));
			pvBuffer += sizeof(XACHIEVEMENT_DETAILS);
		}
	}

	if (pOverlapped == NULL)
	{
		return ERROR_SUCCESS;
	}
	else
	{
		pOverlapped->InternalLow = ERROR_SUCCESS;
		pOverlapped->dwExtendedError = 0;

		return ERROR_IO_PENDING;
	}
}
