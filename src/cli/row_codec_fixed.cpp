// src/cli/row_codec_fixed.cpp

#include "dt/data/row_codec_fixed.hpp"

#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

namespace dt::data {

namespace {

static std::string rtrim_copy(std::string s)
{
    while (!s.empty()) {
        const char c = s.back();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            s.pop_back();
        } else {
            break;
        }
    }
    return s;
}

static std::string digits_only(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            out.push_back(c);
        }
    }
    return out;
}

static std::string fit_left(std::string s, std::size_t width, char fill)
{
    s = rtrim_copy(std::move(s));
    if (s.size() > width) {
        s.resize(width);
    } else if (s.size() < width) {
        s.append(width - s.size(), fill);
    }
    return s;
}

static std::string fit_right(std::string s, std::size_t width, char fill)
{
    s = rtrim_copy(std::move(s));
    if (s.size() > width) {
        s.resize(width);
    } else if (s.size() < width) {
        s.insert(s.begin(), width - s.size(), fill);
    }
    return s;
}

static std::string cell_to_string(const Cell& cell)
{
    if (cell.has_value) {
        if (std::holds_alternative<std::string>(cell.value)) {
            return std::get<std::string>(cell.value);
        }
        if (std::holds_alternative<double>(cell.value)) {
            std::ostringstream oss;
            oss << std::get<double>(cell.value);
            return oss.str();
        }
        if (std::holds_alternative<DateYMD>(cell.value)) {
            const DateYMD d = std::get<DateYMD>(cell.value);
            if (d.is_valid()) {
                std::ostringstream oss;
                oss << std::setw(4) << std::setfill('0') << d.year
                    << std::setw(2) << std::setfill('0') << d.month
                    << std::setw(2) << std::setfill('0') << d.day;
                return oss.str();
            }
            return {};
        }
        if (std::holds_alternative<bool>(cell.value)) {
            return std::get<bool>(cell.value) ? "T" : "F";
        }
    }

    return cell.raw;
}

static std::string format_fixed_value(const Cell& cell, const FixedFieldSpec& spec)
{
    const std::string raw = cell_to_string(cell);

    switch (spec.kind) {
    case FixedFieldKind::Text:
        if (spec.align == FixedAlign::Right)
            return fit_right(raw, spec.width, spec.fill);
        return fit_left(raw, spec.width, spec.fill);

    case FixedFieldKind::Digits: {
        std::string d = digits_only(raw);
        if (spec.align == FixedAlign::Left)
            return fit_left(d, spec.width, spec.fill);
        return fit_right(d, spec.width, spec.fill);
    }

    case FixedFieldKind::DateYYYYMMDD: {
        std::string d;

        if (cell.has_value && std::holds_alternative<DateYMD>(cell.value)) {
            const DateYMD date = std::get<DateYMD>(cell.value);
            if (date.is_valid()) {
                std::ostringstream oss;
                oss << std::setw(4) << std::setfill('0') << date.year
                    << std::setw(2) << std::setfill('0') << date.month
                    << std::setw(2) << std::setfill('0') << date.day;
                d = oss.str();
            }
        }

        if (d.empty()) {
            d = digits_only(raw);
        }
        if (d.size() != 8) {
            d = "00000000";
        }

        if (spec.align == FixedAlign::Left)
            return fit_left(d, spec.width, spec.fill);
        return fit_right(d, spec.width, spec.fill);
    }

    case FixedFieldKind::NumericFixed: {
        double value = 0.0;

        if (cell.has_value && std::holds_alternative<double>(cell.value)) {
            value = std::get<double>(cell.value);
        } else {
            try {
                if (!raw.empty()) {
                    value = std::stod(raw);
                }
            } catch (...) {
                value = 0.0;
            }
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(spec.decimals) << value;
        const std::string s = oss.str();

        if (spec.align == FixedAlign::Left)
            return fit_left(s, spec.width, spec.fill);
        return fit_right(s, spec.width, spec.fill);
    }

    default:
        if (spec.align == FixedAlign::Right)
            return fit_right(raw, spec.width, spec.fill);
        return fit_left(raw, spec.width, spec.fill);
    }
}

} // namespace

bool write_rowset_as_fixed(
    std::ostream& out,
    const RowSet& rowset,
    const FixedProfile& profile,
    std::string* error
)
{
    if (!out) {
        if (error) *error = "output stream is not writable";
        return false;
    }

    std::unordered_map<std::string, std::size_t> column_index;
    for (std::size_t i = 0; i < rowset.schema.columns.size(); ++i) {
        column_index[rowset.schema.columns[i].name] = i;
    }

    for (const auto& row : rowset.rows) {
        for (const auto& spec : profile.fields) {
            auto it = column_index.find(spec.name);
            if (it == column_index.end()) {
                if (error) {
                    *error = "fixed profile field not found in schema: " + spec.name;
                }
                return false;
            }

            const std::size_t idx = it->second;
            if (idx >= row.cells.size()) {
                if (error) {
                    *error = "row cell index out of range for field: " + spec.name;
                }
                return false;
            }

            out << format_fixed_value(row.cells[idx], spec);
            if (!out) {
                if (error) *error = "write failure while emitting fixed record";
                return false;
            }
        }

        out << profile.line_ending;
        if (!out) {
            if (error) *error = "write failure while emitting line ending";
            return false;
        }
    }

    return true;
}

} // namespace dt::data