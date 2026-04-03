// src/dt/predicate/value_convert.cpp
//
// Implementations for dt::predicate value/literal helpers.
//

#include "dt/predicate/value_convert.hpp"
#include "dt/predicate/predicate_engine.hpp"
#include "value_normalize.hpp"

#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

using namespace util;

namespace dt::predicate {

// Small helpers
static std::string trim_copy(const std::string& s) {
    return trim(s);
}

static std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

// ---------------------------
// Constructors
// ---------------------------

Value make_null() {
    Value v;
    v.type = ValueType::Null;
    v.s.clear();
    v.n = 0.0;
    v.b = false;
    return v;
}

Value make_character(const std::string& s) {
    Value v;
    v.type = ValueType::Character;
    v.s    = s;
    v.n    = 0.0;
    v.b    = false;
    return v;
}

Value make_numeric(double n) {
    Value v;
    v.type = ValueType::Numeric;
    v.n    = n;

    std::ostringstream oss;
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    oss.precision(15);
    oss << n;
    v.s = oss.str();

    v.b = false;
    return v;
}

Value make_date_from_yyyymmdd(const std::string& yyyymmdd) {
    Value v;
    v.type = ValueType::Date;
    v.s    = yyyymmdd;
    v.n    = 0.0; // we can add a serial later if needed
    v.b    = false;
    return v;
}

Value make_logical(bool b) {
    Value v;
    v.type = ValueType::Logical;
    v.b    = b;
    v.s    = b ? "T" : "F";
    v.n    = 0.0;
    return v;
}

// ---------------------------
// Coercions
// ---------------------------

bool coerce_to_logical(const Value& v,
                       bool&        out,
                       std::string* error_out) {
    switch (v.type) {
    case ValueType::Logical:
        out = v.b;
        return true;

    case ValueType::Numeric:
        out = (v.n != 0.0);
        return true;

    case ValueType::Character: {
        auto norm = normalize_logical(v.s);
        if (!norm) {
            if (error_out) {
                *error_out = "Cannot coerce character value [" + v.s + "] to logical.";
            }
            return false;
        }
        out = (*norm == "T");
        return true;
    }

    case ValueType::Null:
        if (error_out) {
            *error_out = "Cannot coerce NULL value to logical.";
        }
        return false;

    case ValueType::Date:
        if (error_out) {
            *error_out = "Cannot coerce DATE value to logical.";
        }
        return false;
    }

    if (error_out) {
        *error_out = "Unknown ValueType in coerce_to_logical.";
    }
    return false;
}

// ---------------------------
// Literal + normalized mapping
// ---------------------------

Value normalize_literal_for_field(char              ftype,
                                  int               flen,
                                  int               fdec,
                                  const std::string& raw_literal,
                                  std::string*      error_out) {
    auto norm = normalize_for_compare(ftype, flen, fdec, raw_literal);
    if (!norm) {
        if (error_out) {
            *error_out = "normalize_for_compare rejected literal [" + raw_literal + "].";
        }
        return make_null();
    }
    return value_from_normalized(ftype, flen, fdec, *norm, error_out);
}

Value value_from_normalized(char              ftype,
                            int               flen,
                            int               fdec,
                            const std::string& normalized,
                            std::string*      error_out) {
    (void)flen;
    (void)fdec;

    char kind = static_cast<char>(
        std::toupper(static_cast<unsigned char>(ftype)));

    try {
        switch (kind) {
        case 'C':
            return make_character(normalized);

        case 'N': {
            if (normalized.empty()) {
                if (error_out) {
                    *error_out = "Empty normalized numeric string.";
                }
                return make_null();
            }
            double d = std::stod(normalized);
            return make_numeric(d);
        }

        case 'D':
            // At this stage normalized is already YYYYMMDD (or we trust it).
            return make_date_from_yyyymmdd(normalized);

        case 'L': {
            std::string u = upper_copy(trim_copy(normalized));
            if (u == "T" || u == ".T.") return make_logical(true);
            if (u == "F" || u == ".F.") return make_logical(false);
            if (error_out) {
                *error_out = "Normalized logical value not recognized: [" + normalized + "].";
            }
            return make_null();
        }

        default:
            // Unknown type: fall back to character.
            if (error_out) {
                *error_out = "Unknown field type in value_from_normalized; treating as character.";
            }
            return make_character(normalized);
        }
    } catch (...) {
        if (error_out) {
            *error_out = "Exception while interpreting normalized value [" + normalized + "].";
        }
        return make_null();
    }
}

// ---------------------------
// DBF bridge
// ---------------------------

Value field_value_from_dbf(const RecordContext& rc,
                           const FieldTypeInfo& fti,
                           std::string*         error_out) {
    return get_field_value_normalized(rc, fti, error_out);
}

} // namespace dt::predicate



