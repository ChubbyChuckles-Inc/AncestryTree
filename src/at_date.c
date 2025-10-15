#include "at_date.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static bool is_leap_year(int year)
{
    if ((year % 4) != 0)
    {
        return false;
    }
    if ((year % 100) != 0)
    {
        return true;
    }
    return (year % 400) == 0;
}

static bool parse_component(const char *value, size_t count, int *out_result)
{
    int result = 0;
    for (size_t index = 0; index < count; ++index)
    {
        if (!isdigit((unsigned char)value[index]))
        {
            return false;
        }
        result = result * 10 + (value[index] - '0');
    }
    if (out_result)
    {
        *out_result = result;
    }
    return true;
}

bool at_date_is_valid_iso8601(const char *value)
{
    if (!value)
    {
        return false;
    }
    if (strlen(value) != 10U)
    {
        return false;
    }
    if (value[4] != '-' || value[7] != '-')
    {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;

    if (!parse_component(value, 4U, &year) || !parse_component(value + 5, 2U, &month) ||
        !parse_component(value + 8, 2U, &day))
    {
        return false;
    }

    if (month < 1 || month > 12)
    {
        return false;
    }

    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = days_in_month[month - 1];
    if (month == 2 && is_leap_year(year))
    {
        max_day = 29;
    }

    if (day < 1 || day > max_day)
    {
        return false;
    }

    return true;
}

bool at_date_is_valid_year_or_date(const char *value)
{
    if (!value)
    {
        return false;
    }
    size_t length = strlen(value);
    if (length == 4U)
    {
        for (size_t index = 0; index < 4U; ++index)
        {
            if (!isdigit((unsigned char)value[index]))
            {
                return false;
            }
        }
        return true;
    }
    return at_date_is_valid_iso8601(value);
}
