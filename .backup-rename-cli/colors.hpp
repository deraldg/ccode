#pragma once
#include <string>

namespace cli { namespace colors {

enum class Theme { Default, Green, Amber };

bool enableAnsi();
std::string seqTheme(Theme t, bool use256);
std::string seqReset();
void applyTheme(Theme t);
void reset();
Theme parseTheme(const std::string& s, bool& ok);
bool has256();

}} // namespace cli::colors
