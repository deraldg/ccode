#include "dt/data/fixed_profiles.hpp"

namespace dt::data {

FixedProfile build_students_ro_fixed_profile()
{
    FixedProfile p;
    p.profile_name = "students_ro";
    p.line_ending = "\r\n";

    p.fields = {
        {"SID",      8,  FixedAlign::Right, '0', FixedFieldKind::Digits,       0},
        {"LNAME",   20,  FixedAlign::Left,  ' ', FixedFieldKind::Text,         0},
        {"FNAME",   15,  FixedAlign::Left,  ' ', FixedFieldKind::Text,         0},
        {"DOB",      8,  FixedAlign::Left,  ' ', FixedFieldKind::DateYYYYMMDD, 0},
        {"GENDER",   1,  FixedAlign::Left,  ' ', FixedFieldKind::Text,         0},
        {"MAJOR",    4,  FixedAlign::Left,  ' ', FixedFieldKind::Text,         0},
        {"ENROLL_D", 8,  FixedAlign::Left,  ' ', FixedFieldKind::DateYYYYMMDD, 0},
        {"GPA",      5,  FixedAlign::Right, ' ', FixedFieldKind::NumericFixed, 2},
        {"EMAIL",   40,  FixedAlign::Left,  ' ', FixedFieldKind::Text,         0}
    };

    return p;
}

} // namespace dt::data