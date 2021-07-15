#include <boost/core/demangle.hpp>

#include <iostream>
#include <typeinfo>

#include "bytecode.h"
#include "utils.h"
#include "type_list.h"

namespace detail {

    template<typename T, typename TList> using append_unique_nonvoid_t =
        util::type_list_append_if_t<!std::is_void_v<T> && !util::type_list_contains_v<T, TList>, T, TList>;

    template<typename TList, typename TFrom> struct unique_types{};
    template<typename TList> struct unique_types<TList, util::type_list<>> {
        using type = TList;
    };
    template<typename TList, typename First, typename ... Ts> struct unique_types<TList, util::type_list<First, Ts...>> {
        using type = typename unique_types<append_unique_nonvoid_t<First, TList>, util::type_list<Ts...>>::type;
    };

}

template<typename TList> using unique_types_t = typename detail::unique_types<util::type_list<>, TList>::type;

namespace detail {
    template<enums::type_enum Enum, typename ISeq> struct enum_type_list{};
    template<enums::type_enum Enum, size_t ... Is> struct enum_type_list<Enum, std::index_sequence<Is...>> {
        using type = util::type_list<typename enums::get_type_t<Enum, static_cast<Enum>(Is)> ...>;
    };
}

template<enums::type_enum Enum> using enum_type_list_t = typename detail::enum_type_list<Enum,
    std::make_index_sequence<enums::size<Enum>()>>::type;

template<typename T> struct variant_type_list{};
template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
    using type = util::type_list<Ts...>;
};

template<typename T>
void print_size(std::string name = boost::core::demangle(typeid(T).name())) {
    std::cout << std::format("{:>3} {}\n", sizeof(T), name);
}

template<typename T> struct type_printer{};
template<typename ... Ts> struct type_printer<util::type_list<Ts...>> {
    void operator()() {
        (print_size<Ts>(), ...);
    }
};

int main() {
    print_size<bls::command_args>();

    type_printer<unique_types_t<enum_type_list_t<bls::opcode>>>{}();

    print_size<bls::variable>();
    print_size<bls::variable::variant_type>();

    type_printer<variant_type_list_t<bls::variable::variant_type>>{}();
    
    return 0;
}