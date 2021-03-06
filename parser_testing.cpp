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

TEST(author, task1_sample1)
{
    constexpr auto str = "\\a.\\b.a b c (\\d.e \\f.g) h";
    constexpr auto result = "(\\a.(\\b.((((a b) c) (\\d.(e (\\f.g)))) h)))";

    test_str(str, result);
}

TEST(author, task1_sample2)
{
    constexpr auto str = "((a\\bbb.c)d)e\nf\tg";
    constexpr auto result = "(((((a (\\bbb.c)) d) e) f) g)";

    test_str(str, result);
}

TEST(correctness, userdata_test)
{
    auto x_hash = std::hash<std::string>()("x");

    parsing_context<hashing_userdata> contxt;

    auto ast_rec = contxt.parse_lambda({"x", 1});
    assert(ast_rec->userdata.hashcode == x_hash);

    contxt.reset();
    ast_rec = contxt.parse_lambda({"\\x.x", 4});
    assert(ast_rec->userdata.hashcode == (((((x_hash * 31u) + 42) * 31u) + x_hash ) * 31) + 0xcaffab27);

    contxt.reset();
    ast_rec = contxt.parse_lambda({"x x", 3});
    assert(ast_rec->userdata.hashcode == (((((x_hash * 31u) + 971u) * 31u) + x_hash ) * 31) + 0xcaffab27);
}


int main(int argc, char* argv[])
{
    umask(0);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
