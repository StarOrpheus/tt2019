#include "include/lambdas.h"

#include <sys/resource.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

[[nodiscard]]
static inline
int set_stack_lim()
{
    const rlim_t kStackSize = 32 * 1024 * 1024;   // min stack size = 16 MB
    struct rlimit rl;
    int result;

    result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0)
    {
        if (rl.rlim_cur < kStackSize)
        {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
                fprintf(stderr, "setrlimit returned result = %d\n", result);
        }
    }

    return result;
}

int main()
{
    if (int err = set_stack_lim())
        return err;

    std::vector<char> buff(1024 * 1024, 0);
    size_t sz = 0;

    do
    {
        auto rd = read(0, buff.data() + sz, buff.size() - sz);
        if (rd < 0)
        {
            std::cerr << "Error reading stdin: " << strerror(errno) << std::endl;
            return -1;
        }

        if (rd == 0) break;

        sz += static_cast<size_t>(rd);
    } while (true);

    parsing_context<empty_userdata> contxt;
    auto result = contxt.parse_lambda({buff.data(), sz});

    std::cout << *result << std::endl;

    return 0;
}
