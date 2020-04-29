/*
 *	Parallel.hpp
 *	Version:			b.0.2.0
 *	Created:			27.04.2020
 *	Last update:		29.04.2020
 *	Language standart:	ISO C++17
 *	Developed by:		Dorokhov Dmitry
*/
#pragma once
#ifndef __PARALLEL__
#define __PARALLEL__
#include <type_traits>
#include <functional>
#include <queue>
#include <mutex>
#include <array>

#ifdef _HAS_CXX17
//	calss template TreadPool
template<size_t _Num, typename _Trty = std::thread, typename _Cvty = std::condition_variable,
	typename _Mty = std::mutex, typename _Fty = std::function<void(void)>> class ThreadPool {
	using _Check_thread = std::void_t<decltype(std::declval<_Trty>().join()), decltype(std::declval<_Trty>().detach())>;
	using _Check_cond_car = std::void_t<decltype(std::declval<_Cvty>().notify_all()),
		decltype(std::declval<_Cvty>().wait(std::declval<std::unique_lock<_Mty>&>()))>;
	using _Check_mutex = std::void_t<decltype(std::declval<_Mty>().lock()), decltype(std::declval<_Mty>().unlock())>;
	using _Check_function = std::void_t<decltype(std::declval<_Fty>().operator()())>;
public:
	using thread_type = _Trty;
	using conditional_variable_type = _Cvty;
	using mutex_type = _Mty;
	using function_type = _Fty;
	
	static_assert(std::is_copy_assignable_v<function_type>, "function_type is not copy-assignable!");
	static_assert(std::is_copy_constructible_v<function_type>, "function_type is not copy-constructible!");
	
	ThreadPool() = default;
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	void add_work(const function_type& foo) {
		_queue_mute.lock();
		_work_buffer.push(foo);
		_queue_mute.unlock();
		_cond.notify_all();
	};
	void start_all() {
		if (_Is_active()) return;
		_Activate();
		auto _Invoke_wrapper = [&]() -> void {
			while (_Is_active()) {
				function_type foo; {
					std::unique_lock<mutex_type> lck(_queue_mute);
					while (_work_buffer.empty()) {
						if (!_is_active) return;
						_cond.wait(lck);
					}
					foo = _work_buffer.front();
					_work_buffer.pop();
				}
				foo();
			}
		};
		for (auto& thr : _threads) thr = thread_type(_Invoke_wrapper);
	};
	void stop_all() {
		if (_Is_active()) {
			_Deactivate();
			_cond.notify_all();
			_Join_all();
		}
	};
	inline bool is_active() const noexcept { return _Is_active(); };
	bool empty() const noexcept {
		_queue_mute.lock();
		bool ret = _work_buffer.empty();
		_queue_mute.unlock();
		return ret;
	}
	~ThreadPool() { stop_all(); };
private:
	inline bool _Is_active() const {
		_activation_mute.lock();
		bool ret = _is_active;
		_activation_mute.unlock();
		return ret;
	};
	void _Activate() {
		_activation_mute.lock();
		_is_active = true;
		_activation_mute.unlock();
	};
	void _Deactivate() {
		_activation_mute.lock();
		_is_active = false;
		_activation_mute.unlock();
	};
	void _Join_all() { for (auto& thr : _threads) thr.join(); };

	std::queue<function_type> _work_buffer;
	std::array<thread_type, _Num> _threads;
	conditional_variable_type _cond;
	mutable mutex_type _queue_mute, _activation_mute;
	bool _is_active = false;
};
//	struct template Is_function_suitable_for_thread_pool
template<typename _Funct, typename... _Args> struct Is_function_suitable_for_thread_pool :
	std::bool_constant<std::is_invocable_v<_Funct, _Args...> && std::is_same_v<std::invoke_result_t<_Funct, _Args...>, void>> {};
template<typename _Funct, typename... _Args> inline constexpr bool Is_function_suitable_for_thread_pool_v =
	Is_function_suitable_for_thread_pool<_Funct, _Args...>::value;
template<typename _Funct, typename... _Args> inline constexpr bool Check_is_function_suitable_for_thread_pool(_Funct&& foo, _Args&&... args) {
	return Is_function_suitable_for_thread_pool_v<_Funct, _Args...>;
};
#ifndef __TYPE_TRAITS__
#define __TYPE_TRAITS__
//  struct template indexTuple
template<size_t... _Idxs> struct IndexTuple {};
template<size_t _Num, typename Tp = IndexTuple<>> struct IndexTupleBuilder;
template<size_t _Num, size_t... _Idxs> struct IndexTupleBuilder<_Num, IndexTuple<_Idxs...>> :
	IndexTupleBuilder<_Num - 1, IndexTuple<_Idxs..., sizeof... (_Idxs)>> {};
template<size_t... _Idxs> struct IndexTupleBuilder<0, IndexTuple<_Idxs ...>> {
	using Indexes = typename IndexTuple<_Idxs...>;
};
template<size_t _Num> using NaturalNumbers = typename IndexTupleBuilder<_Num>::Indexes;
#endif // !__TYPE_TRAITS__
//	struct template Tuple_invoker
template<typename... _Args> struct Tuple_invoker {
	template<typename _Funct, size_t... _Idxs> inline static void invoke(_Funct&& foo, std::tuple<_Args...>&& args, IndexTuple<_Idxs...>&&) {
		foo(std::get<_Idxs>(args)...);
	};
};
//	class template OneFunctionThreadPool
template<size_t _Num, typename _Funct, typename... _Args> class OneFunctionThreadPool {
public:
	using thread_type = std::thread;
	using conditional_variable_type = std::condition_variable;
	using mutex_type = std::mutex;
	using function_type = std::function<void(_Args...)>;

	static_assert(Is_function_suitable_for_thread_pool_v<_Funct, _Args...>, "not suitable function type!");

	OneFunctionThreadPool(_Funct&& foo) : _foo(std::move(foo)) {};
	OneFunctionThreadPool(const OneFunctionThreadPool&) = delete;
	OneFunctionThreadPool(OneFunctionThreadPool&&) = delete;

	void add_work(_Args... args) {
		_queue_mute.lock();
		_args_queue.emplace(std::forward<_Args>(args)...);
		_queue_mute.unlock();
		_cond.notify_all();
	};
	void start_all() {
		if (_Is_active()) return;
		_Activate();
		auto _Invoke_wrapper = [&]() -> void {
			while (_Is_active()) {
				function_type foo;
				std::tuple<_Args...> args; {
					std::unique_lock<mutex_type> lck(_queue_mute);
					while (_args_queue.empty()) {
						if (!_is_active) return;
						_cond.wait(lck);
					}
					foo = _foo;
					args = _args_queue.front();
					_args_queue.pop();
				}
				Tuple_invoker<_Args...>::invoke(std::forward<function_type>(foo),
												std::forward<std::tuple<_Args...>>(args), NaturalNumbers<sizeof... (_Args)>());
			}
		};
		for (auto& thr : _threads) thr = thread_type(_Invoke_wrapper);
	};	
	void stop_all() {
		if (_Is_active()) {
			_Deactivate();
			_cond.notify_all();
			_Join_all();
		}
	};
	inline bool is_active() const noexcept { return _Is_active(); };
	bool empty() const noexcept {
		_queue_mute.lock();
		bool ret = _args_queue.empty();
		_queue_mute.unlock();
		return ret;
	}
	~OneFunctionThreadPool() { stop_all(); };
private:
	inline bool _Is_active() const {
		_activation_mute.lock();
		bool ret = _is_active;
		_activation_mute.unlock();
		return ret;
	};
	void _Activate() {
		_activation_mute.lock();
		_is_active = true;
		_activation_mute.unlock();
	};
	void _Deactivate() {
		_activation_mute.lock();
		_is_active = false;
		_activation_mute.unlock();
	};
	void _Join_all() { for (auto& thr : _threads) thr.join(); };

	std::queue<std::tuple<_Args...>> _args_queue;
	std::array<std::thread, _Num> _threads;
	conditional_variable_type _cond;
	mutable mutex_type _queue_mute, _activation_mute;
	function_type _foo;
	bool _is_active = false;
};
//	function template makeOneFunctionThreadPool
template<size_t _Num, typename _Funct, typename... _Args>
constexpr OneFunctionThreadPool<_Num, _Funct, _Args...> makeOneFunctionThreadPool(_Funct&& foo, _Args&&... args) {
	return OneFunctionThreadPool<_Num, _Funct, _Args...>(std::forward<_Funct>(foo));
};
//	struct template Is_thread_pool
template<typename _Thrp, typename = void> struct Is_thread_pool : std::false_type {};
template<typename _Thrp> struct Is_thread_pool<_Thrp, std::void_t<typename _Thrp::thread_type,
	typename _Thrp::conditional_variable_type, typename _Thrp::mutex_type, typename _Thrp::function_type>> : std::true_type {};
template<typename _Thrp> inline constexpr bool Is_thread_pool_v = Is_thread_pool<_Thrp>::value;
//	class template DynamicThreadPool
template<typename _Trty = std::thread, typename _Cvty = std::condition_variable, typename _Mty = std::mutex,
	typename _Fty = std::function<void(void)>> class DynamicThreadPool {
	using _Check_thread = std::void_t<decltype(std::declval<_Trty>().join()), decltype(std::declval<_Trty>().detach())>;
	using _Check_cond_car = std::void_t<decltype(std::declval<_Cvty>().notify_all()),
		decltype(std::declval<_Cvty>().wait(std::declval<std::unique_lock<_Mty>&>()))>;
	using _Check_mutex = std::void_t<decltype(std::declval<_Mty>().lock()), decltype(std::declval<_Mty>().unlock())>;
	using _Check_function = std::void_t<decltype(std::declval<_Fty>().operator()())>;
public:
	using thread_type = _Trty;
	using conditional_variable_type = _Cvty;
	using mutex_type = _Mty;
	using function_type = _Fty;

	static_assert(std::is_copy_assignable_v<function_type>, "function_type is not copy-assignable!");
	static_assert(std::is_copy_constructible_v<function_type>, "function_type is not copy-constructible!");

	explicit DynamicThreadPool(size_t&& num = thread_type::hardware_concurrency()) noexcept {
		if (!num) num = thread_type::hardware_concurrency();
		_threads = std::vector<thread_type>(std::forward<size_t>(num));
	};
	DynamicThreadPool(const DynamicThreadPool&) = delete;
	DynamicThreadPool(DynamicThreadPool&&) = delete;
	void add_work(const function_type& foo) {
		_queue_mute.lock();
		_work_buffer.push(foo);
		_queue_mute.unlock();
		_cond.notify_all();
	};
	void start_all() {
		if (_Is_active()) return;
		_Activate();
		auto _Invoke_wrapper = [&]() -> void {
			while (_Is_active() || !empty()) {
				function_type foo; {
					std::unique_lock<mutex_type> lck(_queue_mute);
					while (_work_buffer.empty()) {
						if (!_is_active) return;
						_cond.wait(lck);
					}
					foo = _work_buffer.front();
					_work_buffer.pop();
				}
				foo();
			}
		};
		for (auto& thr : _threads) thr = thread_type(_Invoke_wrapper);
	};
	void stop_all() {
		if (_Is_active()) {
			_Deactivate();
			_cond.notify_all();
			_Join_all();
		}
	};
	inline bool is_active() const noexcept { return _Is_active(); };
	bool empty() const noexcept {
		_queue_mute.lock();
		bool ret = _work_buffer.empty();
		_queue_mute.unlock();
		return ret;
	}
	void resize(size_t new_size) {
		stop_all();
		_threads.resize(new_size ? new_size : thread_type::hardware_concurrency());
	};
	~DynamicThreadPool() { stop_all(); };
private:
	inline bool _Is_active() const {
		_activation_mute.lock();
		bool ret = _is_active;
		_activation_mute.unlock();
		return ret;
	};
	void _Activate() {
		_activation_mute.lock();
		_is_active = true;
		_activation_mute.unlock();
	};
	void _Deactivate() {
		_activation_mute.lock();
		_is_active = false;
		_activation_mute.unlock();
	};
	void _Join_all() { for (auto& thr : _threads) thr.join(); };

	std::queue<function_type> _work_buffer;
	std::vector<thread_type> _threads;
	conditional_variable_type _cond;
	mutable mutex_type _queue_mute, _activation_mute;
	bool _is_active = false;
};
//	class template DynamicOneFunctionThreadPool
template<typename _Funct, typename... _Args> class DynamicOneFunctionThreadPool {
public:
	using thread_type = std::thread;
	using conditional_variable_type = std::condition_variable;
	using mutex_type = std::mutex;
	using function_type = std::function<void(_Args...)>;

	static_assert(Is_function_suitable_for_thread_pool_v<_Funct, _Args...>, "not suitable function type!");

	DynamicOneFunctionThreadPool(_Funct&& foo, size_t num = thread_type::hardware_concurrency()) :
		_threads(num ? num : thread_type::hardware_concurrency()), _foo(std::move(foo)) {};
	DynamicOneFunctionThreadPool(const DynamicOneFunctionThreadPool&) = delete;
	DynamicOneFunctionThreadPool(DynamicOneFunctionThreadPool&&) = delete;

	void add_work(_Args... args) {
		_queue_mute.lock();
		_args_queue.emplace(std::forward<_Args>(args)...);
		_queue_mute.unlock();
		_cond.notify_all();
	};
	void start_all() {
		if (_Is_active()) return;
		_Activate();
		auto _Invoke_wrapper = [&]() -> void {
			while (_Is_active()) {
				function_type foo;
				std::tuple<_Args...> args; {
					std::unique_lock<mutex_type> lck(_queue_mute);
					while (_args_queue.empty()) {
						if (!_is_active) return;
						_cond.wait(lck);
					}
					foo = _foo;
					args = _args_queue.front();
					_args_queue.pop();
				}
				Tuple_invoker<_Args...>::invoke(std::forward<function_type>(foo),
												std::forward<std::tuple<_Args...>>(args), NaturalNumbers<sizeof... (_Args)>());
			}
		};
		for (auto& thr : _threads) thr = thread_type(_Invoke_wrapper);
	};
	void stop_all() {
		if (_Is_active()) {
			_Deactivate();
			_cond.notify_all();
			_Join_all();
		}
	};
	inline bool is_active() const noexcept { return _Is_active(); };
	bool empty() const noexcept {
		_queue_mute.lock();
		bool ret = _args_queue.empty();
		_queue_mute.unlock();
		return ret;
	}
	void resize(size_t new_size) {
		stop_all();
		_threads.resize(new_size ? new_size : thread_type::hardware_concurrency());
	};
	~DynamicOneFunctionThreadPool() { stop_all(); };
private:
	inline bool _Is_active() const {
		_activation_mute.lock();
		bool ret = _is_active;
		_activation_mute.unlock();
		return ret;
	};
	void _Activate() {
		_activation_mute.lock();
		_is_active = true;
		_activation_mute.unlock();
	};
	void _Deactivate() {
		_activation_mute.lock();
		_is_active = false;
		_activation_mute.unlock();
	};
	void _Join_all() { for (auto& thr : _threads) thr.join(); };

	std::queue<std::tuple<_Args...>> _args_queue;
	std::vector<std::thread> _threads;
	conditional_variable_type _cond;
	mutable mutex_type _queue_mute, _activation_mute;
	function_type _foo;
	bool _is_active = false;
};
//	function template makeDynamicOneFunctionThreadPool
template<typename _Funct, typename... _Args>
constexpr DynamicOneFunctionThreadPool<_Funct, _Args...> makeDynamicOneFunctionThreadPool(size_t&& num, _Funct&& foo, _Args&&... args) {
	return DynamicOneFunctionThreadPool<_Funct, _Args...>(std::forward<_Funct>(foo), std::forward<size_t>(num));
};
//	class template ThreadsafeVariable
template<typename _Ty, typename _Mty = std::mutex> class ThreadsafeVariable {
	using _Check_mutex = std::void_t<decltype(std::declval<_Mty>().lock()), decltype(std::declval<_Mty>().unlock())>;
public:
	using value_type = _Ty;
	using pointer = _Ty*;
	using reference = _Ty&;
	using const_pointer = const pointer;
	using const_reference = const reference;
	using mutex_type = _Mty;
	using locker_type = std::lock_guard<mutex_type>;

	static_assert(std::is_arithmetic_v<value_type>, "type is not arithmetic!");
	static_assert(std::is_default_constructible_v<value_type>, "type is not default-constructible!");
	static_assert(std::is_copy_assignable_v<value_type>, "type is not copy-assignable!");
	static_assert(std::is_copy_constructible_v<value_type>, "type is not copy-constructible!");
	static_assert(std::is_move_assignable_v<value_type>, "type is not move-assignable!");
	static_assert(std::is_move_constructible_v<value_type>, "type is not move-constructible!");

	template<typename _Other> ThreadsafeVariable(const _Other& value) :
		_value(static_cast<value_type>(value)) {};
	template<typename _Other> explicit ThreadsafeVariable(_Other&& value = _Other()) noexcept :
		_value(std::move(static_cast<value_type>(value))) {};
	template<typename _Other> explicit ThreadsafeVariable(const ThreadsafeVariable<_Other>& other) :
		_value(static_cast<value_type>(other.get())) {};
	template<typename _Other> explicit constexpr ThreadsafeVariable(ThreadsafeVariable<_Other>&& other) noexcept :
		_value(std::move(static_cast<value_type>(other.get()))) {};

	template<typename _Other> ThreadsafeVariable& operator = (const _Other& other) { return set(other); };
	template<typename _Other> ThreadsafeVariable& operator = (_Other&& other) noexcept { return set(std::move(other)); };
	template<typename _Other> ThreadsafeVariable& operator = (const ThreadsafeVariable<_Other>& other) { return set(other.get()); };
	template<typename _Other> ThreadsafeVariable& operator = (ThreadsafeVariable<_Other>&& other) noexcept { return set(std::move(other.get())); };

	template<typename _Other> operator _Other() const noexcept { return static_cast<_Other>(get()); };
	template<typename _Other> operator ThreadsafeVariable<_Other>() const noexcept {
		return ThreadsafeVariable<_Other>(std::move(static_cast<_Other>(get()))); };

	inline constexpr value_type get() const noexcept {
		_mute.lock();
		value_type ret = _value;
		_mute.unlock();
		return ret;
	};
	template<typename _Other> inline ThreadsafeVariable& set(const _Other& value) {
		_mute.lock();
		_value = static_cast<value_type>(value);
		_mute.unlock();
		return *this;
	};
	template<typename _Other> inline ThreadsafeVariable& set(_Other&& value = value_type()) noexcept {
		_mute.lock();
		_value = std::move(static_cast<value_type>(std::move(value)));
		_mute.unlock();
		return* this;
	};
private:
	value_type _value;
	mutable mutex_type _mute;
};
//	operator +
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1> && std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator + (const ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() + right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator + (const _Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left + right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator + (const ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() + right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator += (ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return left.set(left.get() + static_cast<_Ty1>(right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator += (ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return left.set(left.get() + static_cast<_Ty1>(right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
_Ty1& operator += (_Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return left + static_cast<_Ty1>(right.get());
};
//	operator -
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator - (const ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() - right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator - (const _Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left - right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator - (const ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() - right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator -= (ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return left.set(left.get() - static_cast<_Ty1>(right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator -= (ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return left.set(left.get() - static_cast<_Ty1>(right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
_Ty1& operator -= (_Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return left - static_cast<_Ty1>(right.get());
};
//	operator *
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator * (const ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() * right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator * (const _Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left * right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator * (const ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() * right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator *= (ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return left.set(left.get() * static_cast<_Ty1>(right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator *= (ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return left.set(left.get() * static_cast<_Ty1>(right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
_Ty1& operator *= (_Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return left * static_cast<_Ty1>(right.get());
};
//	operator /
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator / (const ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() / right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator / (const _Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left / right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>> operator / (const ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return ThreadsafeVariable<std::common_type_t<_Ty1, _Ty2>>(static_cast<std::common_type_t<_Ty1, _Ty2>>(left.get() / right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator /= (ThreadsafeVariable<_Ty1>& left, const ThreadsafeVariable<_Ty2>& right) {
	return left.set(left.get() / static_cast<_Ty1>(right.get()));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
ThreadsafeVariable<_Ty1>& operator /= (ThreadsafeVariable<_Ty1>& left, const _Ty2& right) {
	return left.set(left.get() / static_cast<_Ty1>(right));
};
template<typename _Ty1, typename _Ty2, typename std::enable_if_t<std::is_arithmetic_v<_Ty1>&& std::is_arithmetic_v<_Ty2>>* = nullptr>
_Ty1& operator /= (_Ty1& left, const ThreadsafeVariable<_Ty2>& right) {
	return left / static_cast<_Ty1>(right.get());
};
//	operator <<
template<typename _Os, typename _Ty> _Os& operator << (_Os& os, const ThreadsafeVariable<_Ty>& value) {
	using _Check_os = std::void_t<decltype(std::declval<_Os>().operator << (std::declval<_Ty>()))>;
	return os << value.get();
};
//	operator >>
template<typename _Is, typename _Ty> _Is& operator >> (_Is& is, ThreadsafeVariable<_Ty>& value) {
	using _Check_is = std::void_t<decltype(std::declval<_Is>().operator >> (std::declval<_Ty&>()))>;
	_Ty val; is >> val; value.set(val); return is;
};
//	function template call
template<typename _Funct, typename _Ty, typename std::enable_if_t<std::is_arithmetic_v<_Ty> && std::is_invocable_v<_Funct, const _Ty&>>* = nullptr>
ThreadsafeVariable<std::invoke_result_t<_Funct, _Ty>> call(_Funct&& foo, const ThreadsafeVariable<_Ty>& arg) {
	return ThreadsafeVariable<std::invoke_result_t<_Funct, _Ty>>(foo(arg.get()));
};
//	some default specifiers
using ThreadsafeBool				= ThreadsafeVariable<bool>;
using ThreadsafeChar				= ThreadsafeVariable<char>;
using ThreadsafeSignedChar			= ThreadsafeVariable<signed char>;
using ThreadsafeUnsignedChar		= ThreadsafeVariable<unsigned char>;
using ThreadsafeWcharT				= ThreadsafeVariable<wchar_t>;
#ifdef __cpp_char8_t
using ThreadsafeChar8T				= ThreadsafeVariable<char8_t>;
#endif // __cpp_char8_t
using ThreadsafeChar16T				= ThreadsafeVariable<char16_t>;
using ThreadsafeChar32T				= ThreadsafeVariable<char32_t>;
using ThreadsafeShort				= ThreadsafeVariable<short>;
using ThreadsafeUnsignedShort		= ThreadsafeVariable<unsigned short>;
using ThreadsafeInt					= ThreadsafeVariable<int>;
using ThreadsafeUnsignedInt			= ThreadsafeVariable<unsigned int>;
using ThreadsafeLongInt				= ThreadsafeVariable<long int>;
using ThreadsafeUnsignedLongInt		= ThreadsafeVariable<unsigned long int>;
using ThreadsafeLongLongInt			= ThreadsafeVariable<long long int>;
using ThreadsafeUnsignedLongLongInt = ThreadsafeVariable<unsigned long long int>;
using ThreadsafeFloat				= ThreadsafeVariable<float>;
using ThreadsafeDouble				= ThreadsafeVariable<double>;
using ThreadsafeLongDouble			= ThreadsafeVariable<long double>;
//	class template ThreadsafeObject
template<typename _Objty, typename _Mty = std::mutex, typename std::enable_if_t<!std::is_final_v<_Objty> &&
	!std::is_arithmetic_v<_Objty>>* = nullptr> class ThreadsafeObject : public _Objty, public _Mty {
	using _Check_mutex = std::void_t<decltype(std::declval<_Mty>().lock()), decltype(std::declval<_Mty>().unlock())>;
	using _Base_obj = _Objty;
	using _Base_mty = _Mty;
public:
	using value_type = _Objty;
	using pointer = _Objty*;
	using reference = _Objty&;
	using const_pointer = const pointer;
	using const_reference = const reference;
	using mutex_type = _Mty;
	using locker_type = std::lock_guard<mutex_type>;

	static_assert(std::is_object_v<value_type>, "type is not an object!");
	static_assert(std::is_default_constructible_v<value_type>, "type is not default-constructible!");
	static_assert(std::is_copy_assignable_v<value_type>, "type is not copy-assignable!");
	static_assert(std::is_copy_constructible_v<value_type>, "type is not copy-constructible!");

	explicit ThreadsafeObject(const value_type& value = value_type()) : _Base_obj(value) {};
	template<typename... _Args, typename std::enable_if_t<std::is_constructible_v<value_type, _Args...>>* = nullptr>
	ThreadsafeObject(_Args&&... args) : _Base_obj(std::forward<_Args>(args)...) {};
};
#ifndef __ONLY_USUAL_IOSTREAM__
//	class template ThreadsafeOstream
#include <iostream>
template<typename _Is = std::istream, typename _Os = std::ostream, typename _Mty = std::mutex>
class ThreadsafeIostreamBase : public _Mty {
	using _Check_mutex = std::void_t<decltype(std::declval<_Mty>().lock()), decltype(std::declval<_Mty>().unlock())>;
public:
	using mutex_type = _Mty;
	using locker_type = std::lock_guard<mutex_type>;
	using istream_type = _Is;
	using ostream_type = _Os;

	ThreadsafeIostreamBase() = delete;
	explicit ThreadsafeIostreamBase(ostream_type& os, istream_type& is) : _os(os), _is(is) {};

	template<typename _Ty> ThreadsafeIostreamBase& operator << (const _Ty& value) { _os << value; return *this; };
	template<typename _Ty> ThreadsafeIostreamBase& operator >> (_Ty& value) { _is >> value; return *this; };
	
private:
	ostream_type& _os;
	istream_type& _is;
};
using ThreadsafeIostream = ThreadsafeIostreamBase<>;
ThreadsafeIostream Iostr(std::cout, std::cin);
#endif // !__ONLY_USUAL_IOSTREAM__
//	struct template Is_thread_safe
template<typename _Ty> struct Is_thread_safe : std::false_type {};
template<typename _Ty> struct Is_thread_safe<ThreadsafeVariable<_Ty>> : std::true_type {};
template<typename _Ty> struct Is_thread_safe<ThreadsafeObject<_Ty>> : std::true_type {};
#ifndef __ONLY_USUAL_IOSTREAM__
template<typename _Ty> struct Is_thread_safe<ThreadsafeIostreamBase<_Ty>> : std::true_type {};
template<> struct Is_thread_safe<ThreadsafeIostream> : std::true_type {};
#endif // !__ONLY_USUAL_IOSTREAM__
template<typename _Ty> inline constexpr bool Is_thread_safe_v = Is_thread_safe<_Ty>::value;
#endif // CXX_17
//	----------------------== powered by Dmitry Dorokhov ==----------------------
#endif // !__PARALLEL__