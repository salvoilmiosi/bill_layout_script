#include <boost/core/demangle.hpp>

#include <iostream>
#include <typeinfo>

#include "bytecode.h"
#include "utils.h"
#include "type_list.h"

template<typename T, typename TList> using append_unique_nonvoid_t =
    type_list_append_if_t<!std::is_void_v<T> && !type_list_contains_v<T, TList>, T, TList>;

template<typename TList, typename TFrom> struct unique_types_impl{};
template<typename TList> struct unique_types_impl<TList, type_list<>> {
    using type = TList;
};
template<typename TList, typename First, typename ... Ts> struct unique_types_impl<TList, type_list<First, Ts...>> {
    using type = typename unique_types_impl<append_unique_nonvoid_t<First, TList>, type_list<Ts...>>::type;
};

template<typename TList> using unique_types_t = typename unique_types_impl<type_list<>, TList>::type;

template<string_enum Enum, typename ISeq> struct enum_type_list_impl{};
template<string_enum Enum, size_t ... Is> struct enum_type_list_impl<Enum, std::index_sequence<Is...>> {
    using type = type_list<EnumType<static_cast<Enum>(Is)> ...>;
};
template<string_enum Enum> using enum_type_list_t = typename enum_type_list_impl<Enum, std::make_index_sequence<EnumSize<Enum>>>::type;

template<typename T> struct variant_type_list{};
template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
    using type = type_list<Ts...>;
};

template<typename T>
void print_size(std::string name = boost::core::demangle(typeid(T).name())) {
    std::cout << std::format("{:>3} {}\n", sizeof(T), name);
}

template<typename T> struct type_printer{};
template<typename ... Ts> struct type_printer<type_list<Ts...>> {
    void operator()() {
        (print_size<Ts>(), ...);
    }
};

int main() {
    print_size<command_args>();

    type_printer<unique_types_t<enum_type_list_t<opcode>>>{}();

    print_size<variable>();
    print_size<variable_variant>();

    type_printer<variant_type_list_t<variable_variant>>{}();
    
    return 0;
}