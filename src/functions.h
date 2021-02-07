#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <functional>
#include <span>

#include "variable.h"
#include "lexer.h"

using arg_list = std::span<variable>;

class invalid_numargs : public parsing_error {
private:
    static std::string get_message(const std::string &fun_name, size_t minargs, size_t maxargs) {
        if (maxargs == std::numeric_limits<size_t>::max()) {
            return fmt::format("La funzione {0} richiede almeno {1} argomenti", fun_name, minargs);
        } else if (minargs == maxargs) {
            return fmt::format("La funzione {0} richiede {1} argomenti", fun_name, minargs);
        } else {
            return fmt::format("La funzione {0} richiede {1}-{2} argomenti", fun_name, minargs, maxargs);
        }
    }

public:
    invalid_numargs(const std::string &fun_name, size_t minargs, size_t maxargs, token &tok)
        : parsing_error(get_message(fun_name, minargs, maxargs), tok) {}
};

struct function_handler : std::function<variable(arg_list&&)> {
    std::string name;
    size_t minargs;
    size_t maxargs;
};

const function_handler &find_function(const std::string &name);

#endif