#pragma once

#include <variant>
#include <string>
#include <memory>
#include <ostream>

struct empty_userdata
{};

struct variable_t;

struct binary_operation;

template<typename UD>
struct parsing_context
{
    template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

    using ast_node = std::variant<variable_t, binary_operation>;
    struct ast_record
    {
        UD          userdata;
        ast_node    node;
    };

    using ast_record_ptr = std::unique_ptr<ast_record>;

    struct variable_t
    {
        std::string name;
    };

    enum class node_tag
    {
        FORALL,
        APPLICATION
    };

    struct binary_operation
    {
        binary_operation(node_tag tag, ast_record_ptr lhs, ast_record_ptr rhs)
            : tag(tag),
              args{std::move(lhs), std::move(rhs)}
        {}

        node_tag            tag;
        ast_record_ptr  args[2];
    };



    parsing_context(std::string_view str)
        : tail(std::move(str)),
          current_token(token_type::EMPTY)
    {}

    token_type get_token()
    {
        return current_token;
    }

    token_type read_token()
    {
        skip_ws();

        assert(!tail.empty());

        switch (tail[0])
        {
        case '\\':
            skip_chars();
            read_var();

            skip_ws();
            assert(tail[0] == '.');
            skip_chars();
            return current_token = token_type::FORALL_VARNAME;;
        case 'a'...'z':
            read_var();
            return current_token = token_type::VARNAME;
        case '(':
            skip_chars();
            return current_token = token_type::OPEN_PARANTH;
        case ')':
            skip_chars();
            return current_token = token_type::CLOSING_PARANTH;
        default:
            assert(false && "Unexpected state");
            break;
        }
    }

    std::string& get_varname()
    {
        return varname;
    }

    std::string_view& get_tail()
    {
        return tail;
    }

    ast_record_ptr parse_lambda(std::string_view str)
    {
        return parse_expression(context);
    }

private:

    enum class token_type
    {
        EMPTY,
        FORALL_VARNAME,
        VARNAME,
        OPEN_PARANTH,
        CLOSING_PARANTH,
    };

    void skip_chars(size_t cnt = 1)
    {
        tail = std::string_view(tail.data() + cnt, tail.size() - cnt);
    }

    void skip_ws()
    {
        size_t cnt = 0;
        while (cnt < tail.size()
               && std::isspace(tail[cnt]))
            cnt++;

        skip_chars(cnt);
    }

    constexpr bool is_varname_char(char c) const
    {
        return (c >= 'a' && c <= 'z')
                || std::isspace(c)
                || c == '\'';
    }

    void read_var()
    {
        skip_ws();
        assert(tail[0] >= 'a' && tail[0] <= 'z');
        size_t i = 1;
        for (; i < tail.size()
                && is_varname_char(tail[i]); ++i)
        {}

        varname = tail.substr(0, i);
        skip_chars(i);
    }


    static inline
    ast_record_ptr parse_expression(parsing_context& contxt);


    static inline
    ast_record_ptr parse_variable(parsing_context& contxt)
    {
        auto token = contxt.get_token();

        assert(token == token_type::VARNAME);

        return std::make_unique<ast_record>(UD{},
                                                        variable_t(std::move(contxt.get_varname())));
    }

    static inline
    ast_record_ptr parse_atom(parsing_context& contxt)
    {
        auto token = contxt.get_token();

        switch (token)
        {
        case token_type::OPEN_PARANTH:
        {
            auto result = parse_expression(contxt);
            auto tok = contxt.get_token();
            assert(tok == token_type::CLOSING_PARANTH);
            return result;
        }
        case token_type::VARNAME:
            return parse_variable(contxt);
        default:
            assert(false && "Unexpected token in for atom");
            break;
        }
    }

    static inline
    ast_record_ptr combine_binary_op(std::vector<ast_record_ptr>& args,
                                               node_tag tag)
    {
        while (args.size() > 1)
        {
            ast_record_ptr rhs = std::move(args.back());
            args.pop_back();
            ast_record_ptr lhs = std::move(args.back());
            args.pop_back();

            args.push_back(std::make_unique<ast_record>(UD{},
                                                                  binary_operation(tag,
                                                                                   std::move(lhs),
                                                                                   std::move(rhs))));
        }

        return std::move(args[0]);
    }

    static inline
    ast_record_ptr parse_application(parsing_context& contxt)
    {
        std::vector<ast_record_ptr> applicants;
        applicants.push_back(parse_atom(contxt));

        do
        {
            auto tok = contxt.read_token();

            switch (tok)
            {
            case token_type::OPEN_PARANTH:
            case token_type::VARNAME:
                applicants.push_back(parse_atom(contxt));
            case token_type::FORALL_VARNAME:
            case token_type::CLOSING_PARANTH:
                return combine_binary_op(applicants, node_tag::APPLICATION);
            default:
                assert(false && "Unexpected token for parse_application");
                break;
            }
        } while (true);
    }

    static inline
    ast_record_ptr parse_expression(parsing_context& contxt)
    {
        auto tok = contxt.read_token();
        switch (tok)
        {
        case token_type::FORALL_VARNAME:
        {
            ast_record_ptr lhs
                    = std::make_unique<ast_record>(UD{},
                                                              variable_t(std::move(contxt.get_varname())));
            auto rhs = parse_expression(contxt);

            return std::make_unique<ast_record>(UD{},
                                                          binary_operation(node_tag::FORALL,
                                                                           std::move(lhs),
                                                                           std::move(rhs)));
        }
        default:
            return parse_application(contxt);
        }
    }

private:
    std::string_view    tail;
    token_type          current_token;
    std::string         varname;
};

template<typename UD>
std::ostream& operator<<(std::ostream& out, parsing_context::ast_record const& rec)
{
    std::visit(overloaded(
                   [&out] (variable_t const& var) { out << var.name; },
                   [&out] (binary_operation const& op)
    {
        out << '(';
        switch (op.tag)
        {
        case node_tag::FORALL:
            out << '\\' << *op.args[0] << "." << *op.args[1];
            break;
        case node_tag::APPLICATION:
            out << *op.args[0] << " " << *op.args[1];
        }
        out << ')';
    }), rec.node);

    return out;
}


namespace lambdas
{


}
