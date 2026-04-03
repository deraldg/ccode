// src/browser/browser_session.cpp
#include "browser/browser_session.hpp"

#include <sstream>

namespace browser
{
    namespace
    {
        Session g_session{};
    }

    Session& session()
    {
        return g_session;
    }

    void reset_session()
    {
        g_session = Session{};
    }

    void ensure_session_root(const std::string& root_alias_if_empty)
    {
        if (g_session.root_alias.empty())
            g_session.root_alias = root_alias_if_empty;

        if (!g_session.root_alias.empty())
            g_session.active = true;

        if (g_session.limit <= 0)
            g_session.limit = 10;
    }

    void set_root_alias(const std::string& alias)
    {
        g_session.root_alias = alias;
        g_session.active = !alias.empty();
    }

    const std::string& root_alias()
    {
        return g_session.root_alias;
    }

    void set_limit(int n)
    {
        g_session.limit = (n > 0) ? n : 10;
        g_session.active = true;
    }

    int limit()
    {
        return (g_session.limit > 0) ? g_session.limit : 10;
    }

    const std::vector<std::string>& path()
    {
        return g_session.path;
    }

    void clear_path()
    {
        g_session.path.clear();
        g_session.active = true;
    }

    bool push_path_alias(const std::string& alias)
    {
        if (alias.empty())
            return false;

        g_session.path.push_back(alias);
        g_session.active = true;
        return true;
    }

    bool pop_path_alias()
    {
        if (g_session.path.empty())
            return false;

        g_session.path.pop_back();
        g_session.active = true;
        return true;
    }

    std::string path_string()
    {
        if (g_session.path.empty())
            return "(none)";

        std::ostringstream oss;
        for (std::size_t i = 0; i < g_session.path.size(); ++i)
        {
            if (i) oss << " -> ";
            oss << g_session.path[i];
        }
        return oss.str();
    }
}