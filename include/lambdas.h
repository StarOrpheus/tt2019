#pragma once

#include <variant>
#include <string>
#include <memory>
#include <ostream>
#include <cassert>
#include <vector>
#include <unordered_map>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<typename UD>
struct binary_operation;

struct variable_t
{
    std::string name;

    explicit operator std::string const&()
    {
        return name;
    }
};

enum class node_tag
{
    FORALL,
    APPLICATION
};

template<typename UD>
struct ast_record;

template<typename UD>
using ast_node = std::variant<variable_t, binary_operation<UD>, std::shared_ptr<ast_record<UD>>>;

using rename_map_t = std::unordered_map<std::string, std::string>;

template<typename UD>
using ast_record_ptr = std::unique_ptr<ast_record<UD>>;

template<typename UD>
struct ast_record
{
    explicit ast_record(ast_node<UD> node)
            : node(std::move(node)),
              userdata(this)
    {}

    ast_record(ast_node<UD> node, UD other)
            : node(std::move(node)),
              userdata(std::move(other))
    {}

    ast_record<UD>& child(uint8_t id)
    {
        if (node.index() != 1)
        {
            assert(false);
            return *this;
        }

        return *std::get<1>(node).args[id];
    }

    ast_record_ptr<UD>& child_ptr(uint8_t id)
    {
        if (node.index() != 1)
        {
            assert(false);
            exit(-1);
        }

        return std::get<1>(node).args[id];
    }

    [[nodiscard]]
    bool has_node_tag(node_tag tag)
    {
        switch (node.index())
        {
        case 0:
            return false;
        case 1:
            return std::get<1>(node).tag == tag;
        case 2:
            return std::get<2>(node)->has_node_tag(tag);
        default:
            assert(false);
            return false;
        }
    }

    [[nodiscard]]
    ast_record_ptr<UD> deep_copy() const
    {
        switch (node.index())
        {
        case 0:
        {
            auto& rec = std::get<0>(node);
            return std::make_unique<ast_record<UD>>(ast_node<UD>{rec}, userdata);
        }
        case 1:
        {
            auto& rec = std::get<1>(node);
            return std::make_unique<ast_record<UD>>(ast_node<UD>{rec.deep_copy()}, userdata);
        }
        case 2:
        {
            auto& rec = std::get<2>(node);
            return rec->deep_copy();
        }
        default:
            assert(false);
            return {nullptr};
        }
    }

    friend std::ostream& operator<<(std::ostream& out, ast_record<UD> const& rec)
    {
        switch (rec.node.index())
        {
        case 0:
        {
            out << std::get<0>(rec.node).name;
            break;
        }
        case 1:
        {
            auto& op = std::get<1>(rec.node);
            out << '(';
            switch (op.tag)
            {
            case node_tag::FORALL:
                out << '\\' << *op.args[0] << "." << *op.args[1];
                break;
            case node_tag::APPLICATION:
                out << *op.args[0] << " " << *op.args[1];
                break;
            default:
                assert(false);
            }
            out << ')';
            break;
        }
        case 2:
        {
            auto& ptr = std::get<2>(rec.node);
            out << *ptr;
            break;
        }
        default:
            assert(false && "Unexpected node type!");
        }

        return out;
    }

    ast_node<UD>    node;
    UD              userdata;
};

template<typename UD>
struct binary_operation
{
    binary_operation(node_tag tag, ast_record_ptr<UD> lhs, ast_record_ptr<UD> rhs)
            : tag(tag),
              args{std::move(lhs), std::move(rhs)}
    {}

    [[nodiscard]]
    binary_operation<UD> deep_copy() const
    {
        return binary_operation(tag, args[0]->deep_copy(), args[1]->deep_copy());
    }

    node_tag            tag;
    ast_record_ptr<UD>  args[2];
};

struct empty_userdata
{
    explicit empty_userdata(ast_record<empty_userdata>* owner)
    {
        assert(&owner->userdata == this);
    };
};

using empty_ast_rec = ast_record<empty_userdata>;
using empty_ast_rec_ptr = ast_record_ptr<empty_userdata>;

template<typename UD>
struct parsing_context
{
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

    ast_record_ptr<UD> parse_lambda(std::string_view const& str)
    {
        tail = str;
        return parse_expression();
    }

    void reset()
    {
        tail = std::string_view();
        current_token = token_type ::EMPTY;
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

    [[nodiscard]]
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

    ast_record_ptr<UD> parse_variable()
    {
        auto token = get_token();

        assert(token == token_type::VARNAME);

        return std::make_unique<ast_record<UD>>(variable_t{std::move(get_varname())});
    }

    ast_record_ptr<UD> parse_atom()
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

    ast_record_ptr<UD> combine_binary_op(std::vector<ast_record_ptr<UD>>& args,
                                        node_tag tag)
    {
        switch (tag)
        {
        case node_tag::FORALL:
            while (args.size() > 1)
            {
                ast_record_ptr<UD> rhs = std::move(args.back());
                args.pop_back();
                ast_record_ptr<UD> lhs = std::move(args.back());
                args.pop_back();

                args.push_back(std::make_unique<ast_record<UD>>(binary_operation(tag, std::move(lhs), std::move(rhs))));
            }
        case node_tag::APPLICATION:
            for (size_t l = 0, r = args.size() - 1; l < r; ++l, --r)
                std::swap(args[l], args[r]);

            while (args.size() > 1)
            {
                ast_record_ptr<UD> lhs = std::move(args.back());
                args.pop_back();
                ast_record_ptr<UD> rhs = std::move(args.back());
                args.pop_back();

                args.push_back(std::make_unique<ast_record<UD>>(binary_operation(tag, std::move(lhs), std::move(rhs))));
            }
        }

        return std::move(args[0]);
    }

    ast_record_ptr<UD> parse_application()
    {
        std::vector<ast_record_ptr<UD>> applicants;
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

    ast_record_ptr<UD> parse_expression(bool token_read = false)
    {
        auto tok = (token_read) ? get_token() : read_token();
        if (tok == token_type::FORALL_VARNAME)
        {
            ast_record_ptr<UD> lhs = std::make_unique<ast_record<UD>>(variable_t{std::move(get_varname())});
            auto rhs = parse_expression();

            return std::make_unique<ast_record<UD>>(binary_operation(node_tag::FORALL, std::move(lhs), std::move(rhs)));
        }
        else
            return parse_application();
    }

private:
    std::string_view    tail;
    token_type          current_token;
    std::string         varname;
};

struct hashing_userdata
{
    /**
     * Public "secret" is ridiculous, but anyway
     */
    constexpr static size_t secret = 31;

    explicit hashing_userdata(ast_record<hashing_userdata>* owner)
    {
        assert(&owner->userdata == this);

        std::visit(
                overloaded {
                        [this] (variable_t const& var)
                        {
                            hashcode = std::hash<std::string>()(var.name);
                        },
                        [this] (binary_operation<hashing_userdata> const& op)
                        {
                            hashcode = op.args[0]->userdata.hashcode * 31u;
                            hashcode += (op.tag == node_tag::FORALL ? 42u : 971u);
                            hashcode *= 31u;
                            hashcode += op.args[0]->userdata.hashcode;
                            hashcode *= 31u;
                            hashcode += 0xcaffab27;
                        },
                        [this] (std::shared_ptr<ast_record<hashing_userdata>> const& ptr)
                        {
                            hashcode = ptr->userdata.hashcode * 17 + 911;
                        }
                },
                owner->node
        );
    };

    size_t hashcode{0};
};
