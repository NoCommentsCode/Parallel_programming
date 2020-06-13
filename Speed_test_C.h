/*
 *	Speed_test_C.h
 *	Version:			b.0.1.0
 *	Created:			13.06.2020
 *	Last update:		13.06.2020
 *	Language standart:	C99
 *	Developed by:		Dorokhov Dmitry
*/
#pragma once
#ifndef __SPEED_TEST__
#define __SPEED_TEST__
#include <Windows.h>

unsigned long long int measureWithSavingResult(void* (*foo)(void*), void* param, void* result) {
	unsigned long long int time = GetTickCount64();
	result = foo(param);
	return GetTickCount64() - time;
};

unsigned long long int measureWithoutSavingResult(void* (*foo)(void*), void* param) {
	unsigned long long int time = GetTickCount64();
	foo(param);
	return GetTickCount64() - time;
};

//	----------------------== powered by Dmitry Dorokhov ==----------------------
#endif // !__SPEED_TEST__