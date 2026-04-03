// include/browser/browser_session.hpp
#pragma once

#include <string>
#include <vector>

namespace browser
{
    struct Session
    {
        std::string root_alias;
        int limit = 10;
        std::vector<std::string> path;
        bool active = false;
    };

    Session& session();

    void reset_session();
    void ensure_session_root(const std::string& root_alias_if_empty);

    void set_root_alias(const std::string& alias);
    const std::string& root_alias();

    void set_limit(int n);
    int limit();

    const std::vector<std::string>& path();
    void clear_path();
    bool push_path_alias(const std::string& alias);
    bool pop_path_alias();

    std::string path_string();
}