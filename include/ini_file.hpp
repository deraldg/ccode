#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ini
{
    struct Entry
    {
        std::string key;
        std::string value;
    };

    struct Section
    {
        std::string name;
        std::vector<Entry> entries;
    };

    class IniFile
    {
    public:
        bool load(const std::string& path, std::string* error = nullptr)
        {
            clear();

            std::ifstream in(path, std::ios::in);
            if (!in)
            {
                if (error) *error = "Could not open INI file: " + path;
                return false;
            }

            std::string line;
            std::string currentSection;
            std::size_t lineNo = 0;

            while (std::getline(in, line))
            {
                ++lineNo;

                // Strip CR if present (Windows text safety).
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                const std::string trimmed = trim(line);

                if (trimmed.empty())
                {
                    continue;
                }

                if (trimmed[0] == ';' || trimmed[0] == '#')
                {
                    continue;
                }

                if (trimmed.front() == '[')
                {
                    if (trimmed.back() != ']')
                    {
                        if (error)
                        {
                            *error = "Malformed section header at line " + std::to_string(lineNo);
                        }
                        clear();
                        return false;
                    }

                    currentSection = trim(trimmed.substr(1, trimmed.size() - 2));
                    ensureSection(currentSection);
                    continue;
                }

                const std::size_t eqPos = trimmed.find('=');
                if (eqPos == std::string::npos)
                {
                    if (error)
                    {
                        *error = "Missing '=' at line " + std::to_string(lineNo);
                    }
                    clear();
                    return false;
                }

                const std::string key = trim(trimmed.substr(0, eqPos));
                const std::string value = trim(trimmed.substr(eqPos + 1));

                if (key.empty())
                {
                    if (error)
                    {
                        *error = "Empty key at line " + std::to_string(lineNo);
                    }
                    clear();
                    return false;
                }

                ensureSection(currentSection);
                Section& sec = sections_[sectionIndex_[currentSection]];
                sec.entries.push_back({key, value});
            }

            return true;
        }

        void clear()
        {
            sections_.clear();
            sectionIndex_.clear();
        }

        bool hasSection(const std::string& section) const
        {
            return sectionIndex_.find(section) != sectionIndex_.end();
        }

        bool hasKey(const std::string& section, const std::string& key) const
        {
            const Section* sec = findSection(section);
            if (!sec) return false;

            for (const auto& e : sec->entries)
            {
                if (iequals(e.key, key))
                {
                    return true;
                }
            }
            return false;
        }

        std::string getString(const std::string& section,
                              const std::string& key,
                              const std::string& defaultValue = "") const
        {
            const Section* sec = findSection(section);
            if (!sec) return defaultValue;

            for (const auto& e : sec->entries)
            {
                if (iequals(e.key, key))
                {
                    return e.value;
                }
            }
            return defaultValue;
        }

        int getInt(const std::string& section,
                   const std::string& key,
                   int defaultValue = 0) const
        {
            const std::string s = getString(section, key, "");
            if (s.empty()) return defaultValue;

            try
            {
                return std::stoi(s);
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        bool getBool(const std::string& section,
                     const std::string& key,
                     bool defaultValue = false) const
        {
            const std::string s = toLower(getString(section, key, ""));
            if (s.empty()) return defaultValue;

            if (s == "true" || s == "1" || s == "yes" || s == "on")
            {
                return true;
            }
            if (s == "false" || s == "0" || s == "no" || s == "off")
            {
                return false;
            }

            return defaultValue;
        }

        const std::vector<Section>& sections() const
        {
            return sections_;
        }

    private:
        std::vector<Section> sections_;
        std::map<std::string, std::size_t> sectionIndex_;

        static std::string ltrim(const std::string& s)
        {
            std::size_t i = 0;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
            {
                ++i;
            }
            return s.substr(i);
        }

        static std::string rtrim(const std::string& s)
        {
            if (s.empty()) return s;

            std::size_t i = s.size();
            while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1])))
            {
                --i;
            }
            return s.substr(0, i);
        }

        static std::string trim(const std::string& s)
        {
            return rtrim(ltrim(s));
        }

        static std::string toLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        static bool iequals(const std::string& a, const std::string& b)
        {
            if (a.size() != b.size()) return false;

            for (std::size_t i = 0; i < a.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i])))
                {
                    return false;
                }
            }
            return true;
        }

        void ensureSection(const std::string& section)
        {
            if (sectionIndex_.find(section) != sectionIndex_.end())
            {
                return;
            }

            const std::size_t idx = sections_.size();
            sections_.push_back({section, {}});
            sectionIndex_[section] = idx;
        }

        const Section* findSection(const std::string& section) const
        {
            auto it = sectionIndex_.find(section);
            if (it == sectionIndex_.end())
            {
                return nullptr;
            }
            return &sections_[it->second];
        }
    };
}