// src/cli/cmd_rbrowse.cpp
#include "cli/cmd_rbrowse.hpp"

#include <cctype>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "browser/browser.hpp"
#include "browser/browser_session.hpp"
#include "colors.hpp"
#include "textio.hpp"
#include "xbase.hpp"

namespace
{
    static std::string trim(const std::string& s)
    {
        return textio::trim(s);
    }

    static std::string upper_copy(const std::string& s)
    {
        return textio::up(s);
    }

    static std::string current_area_name(xbase::DbArea& area)
    {
        try {
            const std::string ln = area.logicalName();
            if (!ln.empty())
                return ln;
        } catch (...) {}

        try {
            const std::string nm = area.name();
            if (!nm.empty())
                return nm;
        } catch (...) {}

        return "";
    }

    static void print_help()
    {
        std::cout
            << "ERSATZ syntax\n"
            << "  ERSATZ                 show full browser snapshot using saved path/root\n"
            << "  ERSATZ SHOW            same as ERSATZ\n"
            << "  ERSATZ TREE            show only relation tree\n"
            << "  ERSATZ GRID            show only tuple grid + summary\n"
            << "  ERSATZ STATUS          show current ERSATZ session state\n"
            << "  ERSATZ ROOT [alias]    set/show browser root alias\n"
            << "  ERSATZ OPEN <alias>    push one child alias onto the path\n"
            << "  ERSATZ BACK            pop one alias from the path\n"
            << "  ERSATZ PATH            show current path\n"
            << "  ERSATZ CLEARPATH       clear current path\n"
            << "  ERSATZ LIMIT <n>       set row limit\n"
            << "  ERSATZ REFRESH         rebuild using current root record position\n"
            << "  ERSATZ RESET           clear saved ERSATZ state\n"
            << "  ERSATZ HELP\n";
    }

    static void print_session_status(xbase::DbArea& area)
    {
        browser::ensure_session_root(current_area_name(area));

        std::cout << "ERSATZ STATUS\n";
        std::cout << "  ROOT  : "
                  << (browser::root_alias().empty() ? "(none)" : browser::root_alias())
                  << "\n";
        std::cout << "  LIMIT : " << browser::limit() << "\n";
        std::cout << "  PATH  : " << browser::path_string() << "\n";
    }

    static bool build_snapshot_from_session(xbase::DbArea& area,
                                            browser::BrowserSnapshot& snap)
    {
        browser::ensure_session_root(current_area_name(area));

        browser::BrowserRequest req{};
        req.root_alias = browser::root_alias();
        req.path_aliases = browser::path();
        req.limit = browser::limit();
        req.refresh_only = true;

        return browser::build_browser_snapshot(area, req, snap);
    }

    static void render_tree_only(const browser::BrowserSnapshot& snap)
    {
        using namespace dli::colors;

        std::cout << "ERSATZ TREE\n";
        std::cout << "ROOT: "
                  << (snap.root.alias.empty() ? "(none)" : snap.root.alias)
                  << "\n";
        std::cout << "PATH: ";
        if (snap.path_aliases.empty())
        {
            std::cout << "(none)\n\n";
        }
        else
        {
            for (std::size_t i = 0; i < snap.path_aliases.size(); ++i)
            {
                if (i) std::cout << " -> ";
                std::cout << snap.path_aliases[i];
            }
            std::cout << "\n\n";
        }

        std::cout << "RELATION TREE\n";
        if (snap.relation_tree.root_alias.empty())
        {
            std::cout << "  (none)\n";
            return;
        }

        std::function<void(const browser::RelationNode&, int)> walk;
        walk = [&](const browser::RelationNode& node, int depth)
        {
            std::string indent(static_cast<std::size_t>(depth * 3), ' ');

            if (treeColorEnabled())
                emitTheme(treeThemeForLevel(depth));

            std::cout << indent << "-> " << node.child_alias;

            if (treeColorEnabled())
                emitCurrentTheme();

            if (!node.on_expr.empty())
            {
                std::cout << "   ON ";

                if (treeColorEnabled())
                    emitTheme(treeThemeForLevel(depth));

                std::cout << node.on_expr;

                if (treeColorEnabled())
                    emitCurrentTheme();
            }
            std::cout << "\n";

            for (const auto& c : node.children)
                walk(c, depth + 1);
        };

        if (treeColorEnabled())
            emitTheme(treeThemeForLevel(0));

        std::cout << "  " << snap.relation_tree.root_alias << "\n";

        if (treeColorEnabled())
            emitCurrentTheme();

        if (snap.relation_tree.links.empty())
        {
            std::cout << "  (none)\n";
            return;
        }

        for (const auto& link : snap.relation_tree.links)
            walk(link, 1);
    }

    static void render_grid_only(const browser::BrowserSnapshot& snap)
    {
        browser::BrowserSnapshot mini = snap;
        mini.root = browser::RecordSnapshot{};
        mini.relation_tree = browser::RelationTreeSnapshot{};
        mini.order = browser::OrderSnapshot{};
        mini.command_name = "ERSATZ GRID";
        browser::render_browser_snapshot_console(mini);
    }

    static bool current_tree_contains_child(const std::vector<browser::RelationNode>& nodes,
                                            const std::string& alias_upper)
    {
        for (const auto& n : nodes)
        {
            if (upper_copy(n.child_alias) == alias_upper)
                return true;
        }
        return false;
    }

    static bool validate_next_alias_against_current_tree(xbase::DbArea& area,
                                                         const std::string& alias)
    {
        browser::BrowserSnapshot snap{};
        if (!build_snapshot_from_session(area, snap))
            return false;

        const std::string want = upper_copy(alias);

        if (browser::path().empty())
            return current_tree_contains_child(snap.relation_tree.links, want);

        const std::vector<browser::RelationNode>* level = &snap.relation_tree.links;

        for (const auto& p : browser::path())
        {
            const browser::RelationNode* found = nullptr;
            for (const auto& n : *level)
            {
                if (upper_copy(n.child_alias) == upper_copy(p))
                {
                    found = &n;
                    break;
                }
            }

            if (!found)
                return false;

            level = &found->children;
        }

        return current_tree_contains_child(*level, want);
    }
}

void cmd_RBROWSE(xbase::DbArea& area, std::istringstream& iss)
{
    std::string sub;
    if (!(iss >> sub))
        sub = "SHOW";

    sub = upper_copy(trim(sub));

    if (sub == "HELP")
    {
        print_help();
        return;
    }

    if (sub == "RESET")
    {
        browser::reset_session();
        std::cout << "ERSATZ: session reset.\n";
        return;
    }

    if (sub == "STATUS")
    {
        print_session_status(area);
        return;
    }

    if (sub == "ROOT")
    {
        std::string alias;
        if (!(iss >> alias))
        {
            browser::ensure_session_root(current_area_name(area));
            std::cout << "ERSATZ ROOT: " << browser::root_alias() << "\n";
            return;
        }

        browser::set_root_alias(alias);
        browser::clear_path();
        std::cout << "ERSATZ ROOT set to " << alias << ".\n";
        return;
    }

    if (sub == "LIMIT")
    {
        int n = 0;
        if (!(iss >> n))
        {
            std::cout << "ERSATZ: LIMIT requires a number.\n";
            return;
        }

        browser::set_limit(n);
        std::cout << "ERSATZ LIMIT set to " << browser::limit() << ".\n";
        return;
    }

    if (sub == "PATH")
    {
        browser::ensure_session_root(current_area_name(area));
        std::cout << "ERSATZ PATH: " << browser::path_string() << "\n";
        return;
    }

    if (sub == "CLEARPATH")
    {
        browser::clear_path();
        std::cout << "ERSATZ: path cleared.\n";
        return;
    }

    if (sub == "BACK")
    {
        if (!browser::pop_path_alias())
        {
            std::cout << "ERSATZ: path already empty.\n";
            return;
        }

        std::cout << "ERSATZ PATH: " << browser::path_string() << "\n";
        return;
    }

    if (sub == "OPEN")
    {
        std::string alias;
        if (!(iss >> alias))
        {
            std::cout << "ERSATZ: OPEN requires a child alias.\n";
            return;
        }

        browser::ensure_session_root(current_area_name(area));

        if (!validate_next_alias_against_current_tree(area, alias))
        {
            std::cout << "ERSATZ: alias '" << alias
                      << "' is not a valid next child for the current path.\n";
            return;
        }

        browser::push_path_alias(alias);
        std::cout << "ERSATZ PATH: " << browser::path_string() << "\n";
        return;
    }

    browser::BrowserSnapshot snap{};
    if (!build_snapshot_from_session(area, snap))
    {
        if (!snap.warnings.empty())
        {
            for (const auto& w : snap.warnings)
                std::cout << "ERSATZ: " << w << "\n";
        }
        else
        {
            std::cout << "ERSATZ: build failed";
            if (!snap.status.empty())
                std::cout << " [" << snap.status << "]";
            std::cout << ".\n";
        }
        return;
    }

    if (sub == "TREE")
    {
        render_tree_only(snap);
        return;
    }

    if (sub == "GRID")
    {
        render_grid_only(snap);
        return;
    }

    if (sub == "REFRESH" || sub == "SHOW")
    {
        browser::render_browser_snapshot_console(snap);
        return;
    }

    std::cout << "ERSATZ: unknown subcommand: " << sub << "\n";
    print_help();
}