#include "3rd-party/gtest/gtest.h"
#include "lambdas.h"

#include <sstream>

static inline
void test_str(char const* sample, char const* result)
{
    parsing_context<empty_userdata> contxt;

    auto ast_rec = contxt.parse_lambda({sample, strlen(sample)});

    std::stringstream ss;
    ss << *ast_rec;

    if (ss.str() != result)
    {
        std::cerr << "Bad formatting:" << std::endl
                  << "Exepected: " << result << std::endl
                  << "Got: " << ss.str() << std::endl;
        assert(false);
    }
}

TEST(correctness, primitive_true)
{
    constexpr auto str = "\\a.\\b.a";
    constexpr auto result = "(\\a.(\\b.a))";

    test_str(str, result);
}

TEST(correctness, primitive_false)
{
    constexpr auto str = "\\a.\\b.b";
    constexpr auto result = "(\\a.(\\b.b))";

    test_str(str, result);
}

TEST(correctness, multiple_applicants)
{
    constexpr auto str = "a b c";
    constexpr auto result = "((a b) c)";

    test_str(str, result);
}


TEST(correctness, global_application)
{
    constexpr auto str = "(\\a.\\b.a) (\\a.\\b.b)";
    constexpr auto result = "((\\a.(\\b.a)) (\\a.(\\b.b)))";

    test_str(str, result);
}

TEST(correctness, local_application)
{
    constexpr auto str = "\\d.e \\f.g";
    constexpr auto result = "(\\d.(e (\\f.g)))";

    test_str(str, result);
}

TEST(author, sample1)
{
    constexpr auto str = "\\a.\\b.a b c (\\d.e \\f.g) h";
    constexpr auto result = "(\\a.(\\b.((((a b) c) (\\d.(e (\\f.g)))) h)))";

    test_str(str, result);
}

TEST(author, sample2)
{
    constexpr auto str = "((a\\bbb.c)d)e\nf\tg";
    constexpr auto result = "(((((a (\\bbb.c)) d) e) f) g)";

    test_str(str, result);
}

int main(int argc, char* argv[])
{
    umask(0);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
