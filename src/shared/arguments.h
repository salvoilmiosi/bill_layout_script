#ifndef __ARGUMENTS_H__
#define __ARGUMENTS_H__

#include <wx/string.h>

template<size_t Maxsize>
class arguments {
private:
    const char *data[Maxsize + 1] = {0};
    size_t numargs = 0;

    void add_arg_impl(const char *arg) {
        assert(numargs < Maxsize);
        data[numargs++] = arg;
    }

public:
    template<typename ... Ts>
    arguments(const Ts & ... others) {
        add_args(others ...);
    }

public:
    void add_arg(const char *arg) {
        add_arg_impl(arg);
    }

    void add_arg(const std::string &arg) {
        add_arg_impl(arg.c_str());
    }

    void add_arg(std::string &&arg) = delete;

    void add_arg(const wxString &arg) {
        add_arg_impl(arg.c_str());
    }

    void add_arg(wxString &&arg) = delete;

    template<typename T, typename ... Ts>
    void add_args(const T &first, Ts & ... others) {
        add_arg(first);

        if constexpr(sizeof...(others) != 0) {
            add_args(others ...);
        }
    }

    operator const char **() {
        return data;
    }
};

template<typename ... Ts> arguments(const Ts & ... args) -> arguments<sizeof ... (args)>;

#endif