#ifndef INDICATOR_LIBRARY_TEXT_H
#define INDICATOR_LIBRARY_TEXT_H
#include "model/Message.h"

#include <Core/Problem.h>

/// A library's problem as a message: the key "lib.<code>", each value plain or,
/// for a nested problem, a message of its own (adapted the same way). Text
/// from outside the libraries (code "raw") stays as it is:
/// Message("common.raw").with("text", text).
Message from_library(Core::Problem const& problem);

#endif  // INDICATOR_LIBRARY_TEXT_H
