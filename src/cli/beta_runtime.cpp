#include "foxref.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <string>

namespace {

std::map<std::string, foxref::BetaStatus> g_overrides;

static std::string trim(std::string s)
{
    auto notsp = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

static std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static std::string status_to_word(foxref::BetaStatus st)
{
    switch (st) {
        case foxref::BetaStatus::DONE:     return "DONE";
        case foxref::BetaStatus::DEFERRED: return "DEFERRED";
        case foxref::BetaStatus::OPEN:
        default:                           return "OPEN";
    }
}

static bool word_to_status(const std::string& s, foxref::BetaStatus& out)
{
    const std::string u = upper(trim(s));

    if (u == "OPEN") {
        out = foxref::BetaStatus::OPEN;
        return true;
    }
    if (u == "DONE") {
        out = foxref::BetaStatus::DONE;
        return true;
    }
    if (u == "DEFER" || u == "DEFERRED") {
        out = foxref::BetaStatus::DEFERRED;
        return true;
    }
    return false;
}

} // namespace

namespace foxref {

BetaStatus beta_effective_status(const BetaItem& it)
{
    const std::string key = upper(it.id ? std::string(it.id) : std::string());
    auto f = g_overrides.find(key);
    if (f != g_overrides.end())
        return f->second;
    return it.status;
}

void beta_set_status(std::string_view id, BetaStatus st)
{
    std::string key(id.begin(), id.end());
    key = upper(trim(key));
    if (!key.empty())
        g_overrides[key] = st;
}

void beta_clear_status(std::string_view id)
{
    std::string key(id.begin(), id.end());
    key = upper(trim(key));
    if (!key.empty())
        g_overrides.erase(key);
}

void beta_clear_all_status_overrides()
{
    g_overrides.clear();
}

std::string beta_default_status_path()
{
    return ".beta_status.txt";
}

bool beta_save_overrides(std::string* err)
{
    const std::string path = beta_default_status_path();
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        if (err) *err = "unable to open for write: " + path;
        return false;
    }

    out << "# DotTalk++ beta status overrides\n";
    out << "# Format: BETA-ID=STATUS\n";

    for (const auto& kv : g_overrides) {
        out << kv.first << "=" << status_to_word(kv.second) << "\n";
    }

    if (!out.good()) {
        if (err) *err = "write failed: " + path;
        return false;
    }

    return true;
}

bool beta_load_overrides(std::string* err)
{
    const std::string path = beta_default_status_path();
    std::ifstream in(path);
    if (!in) {
        if (err) *err = "unable to open for read: " + path;
        return false;
    }

    g_overrides.clear();

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);

        if (line.empty()) continue;
        if (line[0] == '#') continue;

        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string id = upper(trim(line.substr(0, pos)));
        std::string stxt = trim(line.substr(pos + 1));

        const BetaItem* it = beta_find(id);
        if (!it)
            continue;

        BetaStatus st = BetaStatus::OPEN;
        if (!word_to_status(stxt, st))
            continue;

        g_overrides[id] = st;
    }

    return true;
}

} // namespace foxref