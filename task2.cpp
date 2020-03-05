#include "include/lambdas.h"

#include <sys/resource.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <sstream>

[[nodiscard]]
static inline
int set_stack_lim()
{
    const rlim_t kStackSize = 32 * 1024 * 1024;   // min stack size = 32 MB
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

[[nodiscard]]
static inline
std::string gen_name()
{
    static size_t counter = 0;
    return "pinus" + std::to_string(counter++);
}

rename_map_t& get_rename_map()
{
    static rename_map_t renames;
    return renames;
}

static inline
void run_substitutions(empty_ast_rec& rec, bool abstraction = false)
{
    switch (rec.node.index())
    {
    case 0:
    {
        auto& name = std::get<0>(rec.node).name;
        auto it = get_rename_map().find(name);

        if (it != get_rename_map().end())
        {
            if (abstraction)
            {
                auto const newname = gen_name();
                it->second = newname;
            }

            name = it->second;
        }
        else
        {
            if (abstraction)
            {
                auto const newname = gen_name();
                get_rename_map().insert(it, {name, newname});
                name = newname;
            }
            else
                // Global varname expected
                (void) 42;
        }

        break;
    }
    case 1:
    {
        auto& op = std::get<1>(rec.node);

        switch (op.tag)
        {
        case node_tag::FORALL:
        {
            std::optional<std::string> before;

            if (op.args[0]->node.index() != 0)
            {
                assert(false);
                return;
            }

            auto const old_name = std::get<0>(op.args[0]->node).name;

            {
                auto it = get_rename_map().find(old_name);
                if (it != get_rename_map().end())
                {
                    before = std::move(it->second);
                    get_rename_map().erase(it);
                }
            }

            run_substitutions(*op.args[0], true);
            run_substitutions(*op.args[1]);

            {
                auto it = get_rename_map().find(old_name);
                if (before)
                    it->second = *before;
                else
                {
                    assert(it != get_rename_map().end());
                    get_rename_map().erase(it);
                }
            }

            break;
        }
        case node_tag::APPLICATION:
            run_substitutions(*op.args[0]);
            run_substitutions(*op.args[1]);
            break;
        }
        break;
    }
    case 2:
    {
        return;
    }
    default:
        assert(false);
    }
}

static inline
void resolve_referrals(empty_ast_rec_ptr& rec_ptr)
{
    switch (rec_ptr->node.index())
    {
    case 0:
        return;
    case 1:
    {
        auto& rec = std::get<1>(rec_ptr->node);
        resolve_referrals(rec.args[0]);
        resolve_referrals(rec.args[1]);
        break;
    }
    case 2:
    {
        auto& ptr = std::get<2>(rec_ptr->node);
        rec_ptr = ptr->deep_copy();
        run_substitutions(*rec_ptr);
        break;
    }
    default:
        assert(false);
        return;
    }
}

static inline void
substitute_with_referral(empty_ast_rec_ptr& rec_ptr, std::string const& varname,
                         std::shared_ptr<empty_ast_rec>& lazy_link)
{
    switch (rec_ptr->node.index())
    {
    case 0:
    {
        auto& rec = std::get<0>(rec_ptr->node);
        if (rec.name == varname)
            rec_ptr->node = lazy_link;
        break;
    }
    case 1:
    {
        auto& rec = std::get<1>(rec_ptr->node);
        if (rec.tag == node_tag::APPLICATION)
            substitute_with_referral(rec.args[0], varname, lazy_link);
        else
        {
            auto& lhs = rec_ptr->child(0);
            if (lhs.node.index() != 0)
            {
                assert(false);
                return;
            }

            auto& abstraction_name = std::get<0>(lhs.node).name;
            if (abstraction_name == varname)
                return;
        }
        substitute_with_referral(rec.args[1], varname, lazy_link);
        break;
    }
    case 2:
    {
        assert(false && "Unresolved referral");
    }
    default:
        assert(false && "Unexpected node type!");
        return;
    }
}

size_t done = 0;

template<typename SmartPtr>
static inline
bool reduce(empty_ast_rec_ptr& root,
            SmartPtr& rec_ptr)
{
    switch (rec_ptr->node.index())
    {
    case 0: return false;
    case 1:
    {
        auto& parent_op = std::get<1>(rec_ptr->node);
        if (parent_op.tag == node_tag::APPLICATION
            && (rec_ptr->child(0).has_node_tag(node_tag::FORALL)
                || (parent_op.args[0]->node.index() == 2
                    && std::get<2>(parent_op.args[0]->node)->has_node_tag(node_tag::FORALL))))
        {
            resolve_referrals(rec_ptr->child_ptr(0));

            std::shared_ptr ptr = std::move(parent_op.args[1]);

            auto& name_holder_rec = rec_ptr->child(0).child(0);
            if (name_holder_rec.node.index() != 0)
            {
                assert(false);
                return false;
            }
            std::string const& varname = std::get<0>(name_holder_rec.node).name;

            substitute_with_referral(rec_ptr->child(0).child_ptr(1), varname, ptr);

            auto newnode = std::move(rec_ptr->child(0).child(1).node);

            rec_ptr->node = std::move(newnode);
            return true;
        }

        if (reduce(root, parent_op.args[0]))
            return true;
        return reduce(root, parent_op.args[1]);
    }
    case 2:
    {
        auto& ptr = std::get<2>(rec_ptr->node);
        return reduce(root, ptr);
    }
    default:
        assert(false && "Unexpected node type");
        return false;
    }
}

int main()
{
    std::ios_base::sync_with_stdio(false);

    if (int err = set_stack_lim())
        return err;

    /**
     * @m   Number of recutions
     * @k   Output lambda after each k reductions
     */
    size_t m, k;
    std::cin >> m >> k;

    std::string line;
    while (line.empty())
        std::getline(std::cin, line);

    parsing_context<empty_userdata> contxt{};

    auto result = contxt.parse_lambda({line.data(), line.size()});

    run_substitutions(*result);
    assert(get_rename_map().empty());
    std::cout << *result << std::endl;

    while (done < m
           && reduce(result, result))
    {
        done++;
        if (done % k == 0)
            std::cout << *result << '\n';
    }

    if (done % k != 0)
        std::cout << *result << std::endl;

    return 0;
}
