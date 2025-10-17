#ifndef DETAIL_CONTENT_BUILDER_H
#define DETAIL_CONTENT_BUILDER_H

#include <stdbool.h>

struct Person;
struct DetailViewContent;

bool detail_view_content_build(const struct Person *person, struct DetailViewContent *out_content);

#endif /* DETAIL_CONTENT_BUILDER_H */
