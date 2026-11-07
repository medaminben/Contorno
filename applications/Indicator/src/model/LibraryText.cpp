#include "model/LibraryText.h"

Message from_library(Core::Problem const& problem) {
    if (problem.code() == "raw") return Message("common.raw").with("text", problem.text());
    Message message("lib." + problem.code());
    for (auto const& value : problem.values()) {
        if (value.nested) {
            message.with(value.name, from_library(*value.nested));
        } else {
            message.with(value.name, value.plain);
        }
    }
    return message;
}
