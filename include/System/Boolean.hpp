// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <stdexcept>
#include <string>

namespace System {

class Boolean {
public:
    static constexpr const char* TrueString  = "True";
    static constexpr const char* FalseString = "False";

    static bool Parse(const std::string& s) {
        if (s == "True"  || s == "true")  return true;
        if (s == "False" || s == "false") return false;
        throw std::invalid_argument("String must be 'True' or 'False'.");
    }

    static bool TryParse(const std::string& s, bool& result) {
        if (s == "True"  || s == "true")  { result = true;  return true; }
        if (s == "False" || s == "false") { result = false; return true; }
        result = false; return false;
    }

    static std::string ToString(bool value) { return value ? "True" : "False"; }
};

} // namespace System
