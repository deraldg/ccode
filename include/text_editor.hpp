#pragma once

#include <string>

namespace text_editor
{

struct EditorConfig
{
    std::string command{"notepad"};
    bool        use_shell{false};
    bool        wait{true};
    std::string temp_dir{};
    std::string temp_extension{".txt"};
    std::string temp_prefix{"dottalk_memo_"};
    bool        keep_temp_files{false};
    std::string encoding{"ansi"};
    std::string line_endings{"native"};
};

std::string find_dottalk_ini();
EditorConfig load_editor_config();

bool edit_text_via_editor(std::string& text, std::string* err = nullptr);

} // namespace text_editor