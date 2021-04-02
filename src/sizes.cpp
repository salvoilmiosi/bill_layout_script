#include <fmt/format.h>
#include <boost/core/demangle.hpp>

#include <typeinfo>

#include "bytecode.h"

template<typename T, typename TList> struct type_in_list{};

template<typename T> struct type_in_list<T, type_list<>> : std::false_type {};

template<typename T, typename First, typename ... Ts>
struct type_in_list<T, type_list<First, Ts...>> :
    std::conditional_t<std::is_same_v<T, First>,
        std::true_type,
        type_in_list<T, type_list<Ts...>>> {};

template<typename T, typename TList> struct add_if_unique{};
template<typename T, typename ... Ts> struct add_if_unique<T, type_list<Ts...>> {
    using type = std::conditional_t<type_in_list<T, type_list<Ts...>>::value || std::is_void_v<T>, type_list<Ts...>, type_list<T, Ts...>>;
};

template<typename TList> struct unique_types{};
template<typename T> using unique_types_t = typename unique_types<T>::type;
template<> struct unique_types<type_list<>> {
    using type = type_list<>;
};
template<typename First, typename ... Ts>
struct unique_types<type_list<First, Ts...>> {
    using type = typename add_if_unique<First, unique_types_t<type_list<Ts...>>>::type;
};

template<string_enum Enum, typename ISeq> struct enum_type_list_impl{};
template<string_enum Enum, size_t ... Is> struct enum_type_list_impl<Enum, std::index_sequence<Is...>> {
    using type = type_list<EnumType<static_cast<Enum>(Is)> ...>;
};
template<string_enum Enum> using enum_type_list = typename enum_type_list_impl<Enum, std::make_index_sequence<EnumSize<Enum>>>::type;

template<typename TList> using unique_types_t = typename unique_types<TList>::type;

template<typename T>
void print_size(std::string name = boost::core::demangle(typeid(T).name())) {
    fmt::print("{:>3} {}\n", sizeof(T), name);
}

int main() {
    using type_list = unique_types_t<enum_type_list<opcode>>;
    print_size<command_args>();

    []<size_t ... Is>(std::index_sequence<Is...>) {
        (print_size<get_nth_t<Is, type_list>>() , ...);
    } (std::make_index_sequence<type_list::size>{});

    print_size<variable>();
    print_size<variable_variant>();

    []<size_t ... Is>(std::index_sequence<Is...>) {
        (print_size<std::variant_alternative_t<Is, variable_variant>>(), ...);
    } (std::make_index_sequence<std::variant_size_v<variable_variant>>{});
    
    return 0;
}