#include "text_editor.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>

namespace fs = std::filesystem;

namespace text_editor
{

//------------------------------------------------------------
// Resolve editor command
//------------------------------------------------------------

std::string resolve_editor()
{
    const char* env;

    env = std::getenv("DOTTALK_EDITOR");
    if (env && *env) return env;

    env = std::getenv("VISUAL");
    if (env && *env) return env;

    env = std::getenv("EDITOR");
    if (env && *env) return env;

#ifdef _WIN32
    return "notepad";
#elif defined(__APPLE__)
    return "open -W -t";
#else
    return "nano";
#endif
}

//------------------------------------------------------------
// Create temp file
//------------------------------------------------------------

std::string make_temp_file()
{
    auto tmp = fs::temp_directory_path();

    auto now = std::chrono::system_clock::now()
        .time_since_epoch()
        .count();

    std::stringstream ss;
    ss << "dottalk_memo_" << now << ".txt";

    fs::path p = tmp / ss.str();
    return p.string();
}

//------------------------------------------------------------
// Launch editor
//------------------------------------------------------------

bool launch_editor(const std::string& editor, const std::string& file)
{
    std::string cmd = editor + " \"" + file + "\"";

    int result = std::system(cmd.c_str());

    return result == 0;
}

//------------------------------------------------------------
// Edit text workflow
//------------------------------------------------------------

bool edit_text(std::string& text)
{
    std::string editor = resolve_editor();
    std::string temp   = make_temp_file();

    // write current memo contents
    {
        std::ofstream out(temp);
        if (!out) return false;
        out << text;
    }

    // launch editor
    if (!launch_editor(editor, temp))
        return false;

    // read edited contents
    {
        std::ifstream in(temp);
        if (!in) return false;

        std::stringstream buffer;
        buffer << in.rdbuf();
        text = buffer.str();
    }

    // remove temp file
    fs::remove(temp);

    return true;
}

}