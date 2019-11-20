#pragma once

#include <variant>
#include <string>
#include <memory>
#include <ostream>
#include <cassert>
#include <vector>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct empty_userdata
{};

template<typename UD>
struct parsing_context
{
    struct variable_t;
    struct binary_operation;

    using ast_node = std::variant<variable_t, binary_operation>;

    struct ast_record
    {
        template<typename UData2>
        ast_record(UData2&& data, ast_node node)
            : userdata(std::forward<UData2>(data)),
              node(std::move(node))
        {}

        friend std::ostream& operator<<(std::ostream& out, ast_record const& rec)
        {
            std::visit(overloaded{
                           [&out] (variable_t const& var) { out << var.name; },
                           [&out] (binary_operation const& op)
            {
                out << '(';
                switch (op.tag)
                {
                case parsing_context<UD>::node_tag::FORALL:
                    out << '\\' << *op.args[0] << "." << *op.args[1];
                    break;
                case parsing_context<UD>::node_tag::APPLICATION:
                    out << *op.args[0] << " " << *op.args[1];
                }
                out << ')';
            }}, rec.node);

            return out;
        }

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

    enum class token_type
    {
        EMPTY,
        FORALL_VARNAME,
        VARNAME,
        OPEN_PARANTH,
        CLOSING_PARANTH,
    };


    parsing_context()
        : current_token(token_type::EMPTY)
    {}

    token_type get_token()
    {
        return current_token;
    }

    token_type read_token()
    {
        skip_ws();

        if (tail.empty())
            return current_token = token_type::EMPTY;

        switch (tail[0])
        {
        case '\\':
            skip_chars();
            read_var();

            skip_ws();
            assert(tail[0] == '.');
            skip_chars();
            return current_token = token_type::FORALL_VARNAME;
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
            return current_token = token_type::EMPTY;
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
        tail = std::move(str);
        return parse_expression();
    }

private:

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
                || c == '\''
                || (c >= '0' && c <= '9');
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


    ast_record_ptr parse_variable()
    {
        auto token = get_token();

        assert(token == token_type::VARNAME);

        return std::make_unique<ast_record>(UD{},
                                            variable_t{std::move(get_varname())});
    }

    ast_record_ptr parse_atom()
    {
        auto token = get_token();

        switch (token)
        {
        case token_type::OPEN_PARANTH:
        {
            auto result = parse_expression();
            auto tok = get_token();
            assert(tok == token_type::CLOSING_PARANTH);
            return result;
        }
        case token_type::VARNAME:
            return parse_variable();
        default:
            assert(false && "Unexpected token in for atom");
            return {nullptr};
        }
    }

    ast_record_ptr combine_binary_op(std::vector<ast_record_ptr>& args,
                                        node_tag tag)
    {
        switch (tag)
        {
        case node_tag::FORALL:
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
        case node_tag::APPLICATION:
            for (size_t l = 0, r = args.size() - 1; l < r; ++l, --r)
                std::swap(args[l], args[r]);

            while (args.size() > 1)
            {
                ast_record_ptr lhs = std::move(args.back());
                args.pop_back();
                ast_record_ptr rhs = std::move(args.back());
                args.pop_back();

                args.push_back(std::make_unique<ast_record>(UD{},
                                                              binary_operation(tag,
                                                                               std::move(lhs),
                                                                               std::move(rhs))));
            }
        }

        return std::move(args[0]);
    }

    ast_record_ptr parse_application()
    {
        std::vector<ast_record_ptr> applicants;
        applicants.push_back(parse_atom());

        do
        {
            auto tok = read_token();

            switch (tok)
            {
            case token_type::OPEN_PARANTH:
            case token_type::VARNAME:
                applicants.push_back(parse_atom());
                break;
            case token_type::FORALL_VARNAME:
                applicants.push_back(parse_expression(1));
            case token_type::CLOSING_PARANTH:
            case token_type::EMPTY:
                return combine_binary_op(applicants, node_tag::APPLICATION);
            default:
                assert(false && "Unexpected token for parse_application");
                break;
            }
        } while (true);
    }

    ast_record_ptr parse_expression(bool token_read = false)
    {
        auto tok = (token_read) ? get_token() : read_token();
        switch (tok)
        {
        case token_type::FORALL_VARNAME:
        {
            ast_record_ptr lhs
                    = std::make_unique<ast_record>(UD{}, variable_t{std::move(get_varname())});
            auto rhs = parse_expression();

            return std::make_unique<ast_record>(UD{},
                                                  binary_operation(node_tag::FORALL,
                                                                   std::move(lhs),
                                                                   std::move(rhs)));
        }
        default:
            return parse_application();
        }
    }

private:
    std::string_view    tail;
    token_type          current_token;
    std::string         varname;
};
