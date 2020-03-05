#include "lambdas.h"

#include <iostream>
#include <utility>
#include <fstream>

static inline
size_t dot_dfs(std::ostream& out, empty_ast_rec const& rec)
{
    switch (rec.node.index())
    {
    case 0:
    {
        auto& node = std::get<0>(rec.node);
        out << "\tx" << &rec
            << " [label=\"" << node.name << "\" color=\"#000d16\"];\n";
        return 1;
    }
    case 1:
    {
        auto& node = std::get<1>(rec.node);
        if (node.tag == node_tag::APPLICATION)
        {
            out << "\tx" << &rec
                << " [label=\"Apply\" color=\"#021d28\"];\n";
        }
        else
        {
            out << "\tx" << &rec
                << " [label=\"Forall\" color=\"#807b77\"];\n";
        }
        for (auto const& arg : node.args)
            out << "\tx" << &rec << " -> x" << arg.get() << ";\n";
        size_t current_hash = dot_dfs(out, *node.args[0]);
        current_hash *= 31;
        current_hash += (node.tag == node_tag::APPLICATION) ? 23 : 1000000007;
        current_hash *= 37;
        current_hash += dot_dfs(out, *node.args[1]);
        return current_hash;
    }
    case 2:
    {
        auto& node = std::get<2>(rec.node);
        out << "\tx" << &rec
            << " [label=\"Referral\" color=\"#b1bac1\"];\n";

        out << "\t" << &rec << " -> " << node.get() << ";\n";
        return dot_dfs(out, *node) * 911;
    }
    default:
        assert(false && "Unreachable code expected!");
        break;
    }
}

static inline
size_t ast_to_dot(empty_ast_rec const& rec)
{
    static int step = 0;
    std::ofstream out("step" + std::to_string(step) + ".dot");
    out << "digraph AST {\n";
    size_t hEsh = dot_dfs(out, rec);
    out << "}\n";
    return hEsh;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: lambda_to_dot OUT_FNAME" << std::endl;
        return -1;
    }

    std::string str;
    std::getline(std::cin, str);

    parsing_context<empty_userdata> contxt{};
    auto result = contxt.parse_lambda({str.data(), str.size()});

    std::ofstream out(argv[1]);
    out << "digraph AST {\n";
    size_t hEsh = dot_dfs(out, *result);
    out << "}\n";

    std::cout << "Tree hash = " << std::hex << hEsh << std::endl;
    return 0;
}