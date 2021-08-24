#include <boost/core/demangle.hpp>

#include <iostream>
#include <typeinfo>

#include "bytecode.h"
#include "utils.h"
#include "type_list.h"
#include "content_view.h"

template<typename T, typename TList> struct is_unique : std::negation<util::type_list_contains<T, TList>> {};
template<typename T> struct is_nonvoid : std::negation<std::is_void<T>> {};

template<typename TList> using unique_types = util::type_list_filter_t<is_unique, TList>;
template<typename TList> using nonvoid_types = util::type_list_filter_t<is_nonvoid, TList>;

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
    type_printer<nonvoid_types<unique_types<util::enum_type_list_t<bls::opcode>>>>{}();

    std::cout << '\n';

    print_size<bls::variable>();
    type_printer<nonvoid_types<util::enum_type_list_t<bls::variable_type>>>{}();

    std::cout << '\n';

    print_size<bls::content_view>();
    
    return 0;
}