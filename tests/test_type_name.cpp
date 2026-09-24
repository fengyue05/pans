#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <pans/type_name.h>

namespace {
struct AnyStruct{};

enum class AnyEnum : unsigned int
{
    Debug,
    Release,
};

template <typename T, int Size>
struct FixedArray
{
    T values[Size];
};


template <typename T>
void PrintType(std::string_view label)
{
    constexpr std::string_view type_name = pans::TypeName<T>();
    static_assert(!type_name.empty());
    std::cout << label << " -> " << type_name << std::endl;
}
    
}

int main ()
{
    static_assert(pans::TypeName<int>() == "int");
    static_assert(pans::TypeName<double>() == "double");
    static_assert(pans::TypeName<std::vector<int>>() == "std::vector<int>");

    using Counter = int;
    static_assert(pans::TypeName<Counter>() == pans::TypeName<int>());

    constexpr std::string_view first = pans::TypeName<AnyStruct>();
    constexpr std::string_view second = pans::TypeName<AnyStruct>();
    static_assert(first == second);

    assert(first.data() == second.data());

    static_assert(pans::TypeName<const int&>() != pans::TypeName<int>());
    static_assert(pans::TypeName<int*>() != pans::TypeName<int>());
    static_assert(pans::TypeName<int[3]>() != pans::TypeName<int>());

    PrintType<int>("int");
    PrintType<const int>("const int");
    PrintType<const int&>("const int&");
    PrintType<int*>("int pointer");
    PrintType<int[3]>("int array");
    PrintType<std::string>("std::string");
    PrintType<std::vector<int>>("std::vector<int>");
    PrintType<AnyStruct>("user-defined type");
    PrintType<AnyEnum>("enum");
    PrintType<FixedArray<long, 4>>("class template");
    PrintType<int(*)(double)>("function pointer");

    const auto lambda = [](int value){ return value + 1;};
    PrintType<decltype(lambda)>("lambda");

    std::cout << "alias Counter - >" << pans::TypeName<Counter>() << " (会被还原成本来类型)" << std::endl;
    return 0;
}