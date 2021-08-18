#ifndef __TYPE_LIST_H__
#define __TYPE_LIST_H__

#include <type_traits>

namespace util {

    template<typename ... Ts> struct type_list {
        static constexpr size_t size = sizeof...(Ts);
    };

    template<size_t N, typename TList> struct get_nth {};
    template<size_t I, typename TList> using get_nth_t = typename get_nth<I, TList>::type;

    template<size_t N, typename First, typename ... Ts> struct get_nth<N, type_list<First, Ts...>> {
        using type = typename get_nth<N-1, type_list<Ts ...>>::type;
    };

    template<typename First, typename ... Ts> struct get_nth<0, type_list<First, Ts ...>> {
        using type = First;
    };

    template<typename T, typename TList> struct type_list_contains{};
    template<typename T, typename TList> static constexpr bool type_list_contains_v = type_list_contains<T, TList>::value;

    template<typename T> struct type_list_contains<T, type_list<>> : std::false_type {};

    template<typename T, typename ... Ts> struct type_list_contains<T, type_list<T, Ts...>> : std::true_type {};
    template<typename T, typename First, typename ... Ts>
    struct type_list_contains<T, type_list<First, Ts...>> : type_list_contains<T, type_list<Ts...>> {};

    template<typename T, typename TList> struct type_list_indexof{};
    template<typename T, typename TList> static constexpr size_t type_list_indexof_v = type_list_indexof<T, TList>::value;

    template<typename T, typename First, typename ... Ts> struct type_list_indexof<T, type_list<First, Ts...>> {
        static constexpr size_t value = 1 + type_list_indexof_v<T, type_list<Ts ...>>;
    };
    template<typename T, typename ... Ts> struct type_list_indexof<T, type_list<T, Ts...>> {
        static constexpr size_t value = 0;
    };

    template<typename T, typename TList> struct type_list_append{};
    template<typename T, typename TList> using type_list_append_t = typename type_list_append<T, TList>::type;

    template<typename T, typename ... Ts> struct type_list_append<T, type_list<Ts...>> {
        using type = type_list<Ts..., T>;
    };

    template<bool Cond, typename T, typename TList> using type_list_append_if_t =
        std::conditional_t<Cond, type_list_append_t<T, TList>, TList>;

    template<typename T> struct variant_type_list{};
    template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

    template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
        using type = util::type_list<Ts...>;
    };

}

#endif