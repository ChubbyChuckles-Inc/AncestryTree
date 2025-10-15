#ifndef AT_DATE_H
#define AT_DATE_H

#include <stdbool.h>

bool at_date_is_valid_iso8601(const char *value);
bool at_date_is_valid_year_or_date(const char *value);

#endif /* AT_DATE_H */
