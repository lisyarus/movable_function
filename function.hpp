#pragma once

#include <type_traits>
#include <memory>
#include <functional>

template <typename Signature>
struct function;

template <typename R, typename ... Args>
struct function<R(Args...)>
{
private:
	static constexpr std::size_t storage_size = sizeof(void*) * 3;
	static constexpr std::size_t storage_align = alignof(void*);

	template <typename T>
	struct uses_static_storage
		: std::bool_constant<true
			&& sizeof(T) <= storage_size
			&& alignof(T) <= storage_align
			&& ((storage_align % alignof(T)) == 0)
			&& std::is_nothrow_move_constructible_v<T>
			>
	{};

public:

	using signature = R(Args...);

	function() noexcept = default;

	template <typename F>
	function(F && f);

	function (function &&) noexcept;
	function & operator = (function &&) noexcept;

	function (function const &) = delete;
	function & operator = (function const &) = delete;

	~function();

	// operator = (F && f) has strong exception guarantree:
	// if the assignment throws, the function remains unchanged

	template <typename F>
	std::enable_if_t<uses_static_storage<std::decay_t<F>>::value, function &>
		operator = (F && f);

	template <typename F>
	std::enable_if_t<!uses_static_storage<std::decay_t<F>>::value, function &>
		operator = (F && f);

	explicit operator bool() const
	{
		return static_cast<bool>(vtable_);
	}

	template <typename ... Args1>
	R operator()(Args1 && ... args) const;

	void reset();

	void swap(function & other) noexcept;

private:
	std::aligned_storage_t<storage_size, storage_align> storage_;

	struct vtable
	{
		using move_func = void(*)(void *, void *);
		using destroy_func = void(*)(void *);
		using call_func = R(*)(void *, Args ...);

		move_func move;
		destroy_func destroy;
		call_func call;
	};

	vtable * vtable_ = nullptr;

	template <typename F>
	void assign(F && f);
};

template <typename R, typename ... Args>
template <typename F>
function<R(Args...)>::function(F && f)
{
	assign(std::forward<F>(f));
}

template <typename R, typename ... Args>
template <typename F>
std::enable_if_t<function<R(Args...)>::template uses_static_storage<std::decay_t<F>>::value, function<R(Args...)> &>
	function<R(Args...)>::operator = (F && f)
{
	reset();
	assign(std::forward<F>(f));
	return *this;
}

template <typename R, typename ... Args>
template <typename F>
std::enable_if_t<!function<R(Args...)>::template uses_static_storage<std::decay_t<F>>::value, function<R(Args...)> &>
	function<R(Args...)>::operator = (F && f)
{
	function(std::forward<F>(f)).swap(*this);
	return *this;
}

template <typename R, typename ... Args>
function<R(Args...)>::function (function && other) noexcept
{
	vtable_ = other.vtable_;
	if (vtable_)
		vtable_->move(&other.storage_, &storage_);
	other.reset();
}

template <typename R, typename ... Args>
function<R(Args...)> & function<R(Args...)>::operator = (function && other) noexcept
{
	if (this == &other)
		return *this;

	reset();
	vtable_ = other.vtable_;
	if (vtable_)
		vtable_->move(&other.storage_, &storage_);
	other.reset();
	return *this;
}

template <typename R, typename ... Args>
function<R(Args...)>::~function()
{
	reset();
}

template <typename R, typename ... Args>
template <typename ... Args1>
R function<R(Args...)>::operator()(Args1 && ... args) const
{
	if (!vtable_)
		throw std::bad_function_call();

	return vtable_->call(const_cast<void *>(static_cast<void const *>(&storage_)), std::forward<Args1>(args)...);
}

template <typename R, typename ... Args>
void function<R(Args...)>::reset()
{
	if (!vtable_) return;

	vtable_->destroy(&storage_);
	vtable_ = nullptr;
}

template <typename R, typename ... Args>
void function<R(Args...)>::swap(function & other) noexcept
{
	std::swap(*this, other);
}

template <typename R, typename ... Args>
template <typename F>
void function<R(Args...)>::assign(F && f)
{
	using T = std::decay_t<F>;

	if constexpr (uses_static_storage<T>::value)
	{
		new (reinterpret_cast<T *>(&storage_)) T(std::move(f));

		static vtable m = {
			[](void * src, void * dst){ new (reinterpret_cast<T*>(dst)) T(std::move(*reinterpret_cast<T*>(src))); },
			[](void * src){ reinterpret_cast<T*>(src)->~T(); },
			[](void * src, Args ... args) -> R { return std::invoke(*reinterpret_cast<T*>(src), static_cast<Args&&>(args)...); }
		};

		vtable_ = &m;
	}
	else
	{
		*reinterpret_cast<T**>(&storage_) = new T(std::move(f));

		static vtable m = {
			[](void * src, void * dst){ *reinterpret_cast<T**>(dst) = *reinterpret_cast<T**>(src); *reinterpret_cast<T**>(src) = nullptr; },
			[](void * src){ delete *reinterpret_cast<T**>(src); },
			[](void * src, Args ... args) -> R { return std::invoke(**reinterpret_cast<T**>(src), static_cast<Args&&>(args)...); }
		};

		vtable_ = &m;
	}
}
