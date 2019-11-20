#include "include/lambdas.h"


#include <cstring>
#include <unistd.h>
#include <iostream>

int main()
{
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
