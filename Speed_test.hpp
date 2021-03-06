/*
 *	Speed_test.hpp
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

#if _HAS_CXX17
//	function template measureWithSavingResult
template<typename _Time, typename _Callable, typename... _Args>
std::enable_if_t<std::is_invocable_v<_Callable, _Args...>, std::conditional_t<std::is_void_v<
	std::invoke_result_t<_Callable, _Args...>>, _Time, std::pair<_Time, std::invoke_result_t<_Callable, _Args...>>>>
measureWithSavingResult(_Callable&& foo, _Args&&... args) noexcept(std::is_nothrow_invocable_v<_Callable, _Args...>) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	if constexpr (std::is_void_v<std::invoke_result_t<_Callable, _Args...>>) {
		foo(std::forward<_Args>(args)...);
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<_Time>(end - start);
	} else {
		std::invoke_result_t<_Callable, _Args...> result = foo(std::forward<_Args>(args)...);
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		return std::pair<_Time, std::invoke_result_t<_Callable, _Args...>>(std::chrono::duration_cast<_Time>(end - start), result);
	}
};
//	function template measureWithoutSavingResult
template<typename _Time, typename _Callable, typename... _Args>
std::enable_if_t<std::is_invocable_v<_Callable, _Args...>, _Time> measureWithoutSavingResult(_Callable&& foo, _Args&&... args)
	noexcept(std::is_nothrow_invocable_v<_Callable, _Args...>) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	foo(std::forward<_Args>(args)...);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<_Time>(end - start);
};
#else
//	function template measureWithSavingResult
template<typename _Time, typename _Callable, typename... _Args>
std::enable_if_t<std::is_void_v<decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>, _Time>
measureWithSavingResult(_Callable&& foo, _Args&&... args)
noexcept(noexcept(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	foo(std::forward<_Args>(args)...);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	return std::chrono::duration_cast<_Time>(end - start);
};
template<typename _Time, typename _Callable, typename... _Args>
std::enable_if_t<!std::is_void_v<decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>, 
std::pair<_Time, decltype(std::declval<_Callable>()(std::forward<_Args>(std::declval<_Args>())...))>>
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
#endif // _HAS_CXX17
//	----------------------== powered by Dmitry Dorokhov ==----------------------
#endif // !__SPEED_TEST__