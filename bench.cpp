#include "function.hpp"

#include <functional>
#include <string_view>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <array>

template <typename F, typename InitFunc, typename TestFunc>
void do_test_impl(std::string const & name, F & f, InitFunc && init, TestFunc && test)
{
	static const int N = 100000000;
	using clock = std::chrono::high_resolution_clock;
	auto start = clock::now();
	init(f);
	for (int i = 0; i < N; ++i)
		test(f);
	auto end = clock::now();
	std::cout << std::left << std::setw(50) << name << " took " << std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() << "s" << std::endl;
}

template <typename Signature, typename InitFunc, typename TestFunc>
void do_test(std::string const & name, InitFunc && init, TestFunc && test)
{
	{
		std::function<Signature> f;
		do_test_impl(name + " (std::function)", f, std::forward<InitFunc>(init), std::forward<TestFunc>(test));
	}

	{
		function<Signature> f;
		do_test_impl(name + " (function)", f, std::forward<InitFunc>(init), std::forward<TestFunc>(test));
	}
}

int global_counter = 0;

void func()
{
	++global_counter;
}

struct X
{
	int counter = 0;

	void func()
	{
		++counter;
	}
};

int main()
{
	int counter;

	counter = 0;
	do_test<void(int)>("Assign light lambda", [](auto &){}, [&counter](auto & f){ f = [&counter](int a){ counter += a; }; });
	do_test<void(int)>("Call light lambda", [&counter](auto & f){ f = [&counter](int a){ counter += a; }; }, [](auto & f){ f(1); });
	std::cout << "counter = " << counter << std::endl;

	using heavy_type = std::array<int, 256>;

	counter = 0;
	do_test<void(int)>("Assign heavy lambda", [](auto &){}, [&counter](auto & f){ f = [&counter, x = heavy_type{}](int a){ counter += a; }; });
	do_test<void(int)>("Call heavy lambda", [&counter](auto & f){ f = [&counter, x = heavy_type{}](int a){ counter += a; }; }, [](auto & f){ f(1); });
	std::cout << "counter = " << counter << std::endl;

	global_counter = 0;
	do_test<void()>("Assign function pointer", [](auto &){}, [](auto & f){ f = &func; });
	do_test<void()>("Call function pointer", [](auto & f){ f = &func; }, [](auto & f){ f(); });
	std::cout << "global_counter = " << global_counter << std::endl;

	X x;
	do_test<void(X*)>("Assign pointer to member", [](auto &){}, [&x](auto & f){ f = &X::func; });
	do_test<void(X*)>("Call pointer to member", [](auto & f){ f = &X::func; }, [&x](auto & f){ f(&x); });
	std::cout << "x.counter = " << x.counter << std::endl;
}
