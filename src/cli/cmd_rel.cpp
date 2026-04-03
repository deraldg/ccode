// src/cli/cmd_rel.cpp
// REL command dispatcher. Keeps REL subcommand parsing in one place and forwards
// to the underlying RELATIONS / SET RELATIONS / JOIN / ENUM handlers.

#include "cmd_rel.hpp"

#include "cmd_relations.hpp"   // cmd_RELATIONS_LIST/REFRESH/SAVE/LOAD, cmd_REL_JOIN, cmd_REL_ENUM
#include "set_relations.hpp"   // cmd_SET_RELATIONS + relations_api helpers
#include "textio.hpp"

#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

namespace {

static void rel_usage() {
    std::cout
        << "REL syntax\n"
        << "  REL LIST [ALL]\n"
        << "  REL REFRESH\n"
        << "  REL JOIN [LIMIT <n>] [<child1> <child2> ...] TUPLE <expr>\n"
        << "  REL ENUM [LIMIT <n>] [<child1> <child2> ...] TUPLE <expr>\n"
        << "  REL SAVE [path] | REL SAVE AS <dataset>\n"
        << "  REL LOAD [path] | REL LOAD AS <dataset>\n"
        << "  REL ADD <parent> <child> ON <field>      # alias of SET RELATIONS ADD\n"
        << "  REL CLEAR <parent>|ALL                   # alias of SET RELATIONS CLEAR\n";
}

static std::string up(std::string s) { return textio::up(std::move(s)); }

} // namespace

void cmd_REL(xbase::DbArea& area, std::istringstream& in) {
    std::string sub;
    if (!(in >> sub)) {
        rel_usage();
        return;
    }
    sub = up(sub);

    if (sub == "LIST") {
        // REL LIST           -> existing one-hop display (via cmd_RELATIONS_LIST)
        // REL LIST ALL       -> recursive tree display (engine-side, cursor-safe)
        const std::streampos pos = in.tellg();
        std::string maybe;

        if (in >> maybe) {
            const std::string flag = up(maybe);
            if (flag == "ALL") {
                auto rows = relations_api::list_tree_for_current_parent(/*recursive=*/true, /*max_depth=*/24);
                if (rows.empty()) {
                    std::cout << "(no relations)\n";
                    return;
                }

                std::cout << "Relations (tree) rooted at: " << rows[0].line << "\n";
                for (std::size_t i = 0; i < rows.size(); ++i) {
                    std::cout << rows[i].line << "\n";
                }
                return;
            }
        }

        // Restore stream position if we consumed a non-ALL token (or hit EOF).
        in.clear();
        if (pos != std::streampos(-1)) {
            in.seekg(pos);
        }

        cmd_RELATIONS_LIST(area, in);
        return;
    }
    if (sub == "REFRESH") {
        cmd_RELATIONS_REFRESH(area, in);
        return;
    }
    if (sub == "SAVE") {
        cmd_REL_SAVE(area, in);
        return;
    }
    if (sub == "LOAD") {
        cmd_REL_LOAD(area, in);
        return;
    }
    if (sub == "JOIN") {
        cmd_REL_JOIN(area, in);
        return;
    }
    if (sub == "ENUM") {
        cmd_REL_ENUM(area, in);
        return;
    }

    // Aliases to SET RELATIONS
    if (sub == "ADD" || sub == "CLEAR") {
        std::string rest;
        std::getline(in, rest);
        std::istringstream tmp(sub + rest);
        cmd_SET_RELATIONS(area, tmp);
        return;
    }

    rel_usage();
}
