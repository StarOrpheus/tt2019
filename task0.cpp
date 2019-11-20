#include "include/lambdas.h"

#include <vector>
#include <unistd.h>

int main()
{
    std::vector<char> buff(1024 * 1024, 0);

    auto rd = read(0, buff.data(), buff.size());

    if (rd <= 0)
        return -1;

    auto urd = static_cast<size_t>(rd);
    auto ast_rec = lambdas::parse_lambda<lambdas::empty_ud>(std::string_view(buff.data(), urd));

    return 0;
}
