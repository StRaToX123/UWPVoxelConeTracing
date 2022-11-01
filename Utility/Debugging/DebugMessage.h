#pragma once


#include <stdio.h>



template<typename... T>
static void DisplayDebugMessage(char* pFormatString, T... variables)
{
	static char debugMessageBuffer[512];
	sprintf_s(debugMessageBuffer, pFormatString, variables...);
	OutputDebugStringA(debugMessageBuffer);
}

template<typename... T>
static void DisplayDebugMessage(const char* pFormatString, T... variables)
{
	static char debugMessageBuffer[512];
	sprintf_s(debugMessageBuffer, pFormatString, variables...);
	OutputDebugStringA(debugMessageBuffer);
}

