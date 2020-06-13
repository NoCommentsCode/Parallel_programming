/*
 *	Speed_test_lite.hpp
 *	Version:			b.0.1.0
 *	Created:			13.06.2020
 *	Last update:		13.06.2020
 *	Language standart:	ISO C++11
 *	Developed by:		Dorokhov Dmitry
*/
#pragma once
#ifndef __SPEED_TEST__
#define __SPEED_TEST__
#include <chrono>
#include <map>

//	function template measureWithSavingResult
template<typename _Time, typename _Callable, typename... _Args>
typename std::enable_if<std::is_void<decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>::value, _Time>::type
measureWithSavingResult(_Callable&& foo, _Args&&... args)
noexcept(noexcept(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	foo(std::forward<_Args>(args)...);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<_Time>(end - start);
};
template<typename _Time, typename _Callable, typename... _Args>
typename std::enable_if<!std::is_void<decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>::value,
std::pair<_Time, decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>>::type
measureWithSavingResult(_Callable&& foo, _Args&&... args)
noexcept(noexcept(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto result = foo(std::forward<_Args>(args)...);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	return std::pair<_Time, decltype(result)>(std::chrono::duration_cast<_Time>(end - start), result);
};
//	function template measureWithoutSavingResult
template<typename _Time, typename _Callable, typename... _Args>
_Time measureWithoutSavingResult(_Callable&& foo, _Args&&... args)
noexcept(noexcept(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	foo(std::forward<_Args>(args)...);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<_Time>(end - start);
};

//	----------------------== powered by Dmitry Dorokhov ==----------------------
#endif // !__SPEED_TEST__
