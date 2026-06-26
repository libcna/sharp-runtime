// SPDX-License-Identifier: MIT
// Copyright (c) Robert Vokac and contributors
// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)
#pragma once
#include <string>
#include "System/IO/TextReader.hpp"

namespace System::IO {

    /**
     * @brief Implements a TextReader that reads from a string.
     *
     * Partial C++ counterpart of .NET System.IO.StringReader.
     *
     * @note Status: Implemented
     */
    class StringReader : public TextReader {
        std::string s_;
        intcs pos_ = 0;
    public:
        /** Constructs a StringReader over the given string. */
        explicit StringReader(const std::string& s) : s_(s) {}

        /** Returns the next character without advancing the position, or -1 at end. */
        [[nodiscard]] intcs Peek() override {
            if (pos_ >= static_cast<intcs>(s_.size())) return -1;
            return static_cast<unsigned char>(s_[pos_]);
        }

        /** Reads and returns the next character, or -1 at end. */
        intcs Read() override {
            if (pos_ >= static_cast<intcs>(s_.size())) return -1;
            return static_cast<unsigned char>(s_[pos_++]);
        }

        /** Reads the next line, stripping the line terminator. */
        [[nodiscard]] std::string ReadLine() override {
            if (pos_ >= static_cast<intcs>(s_.size())) return "";
            auto start = pos_;
            while (pos_ < static_cast<intcs>(s_.size()) && s_[pos_] != '\n') ++pos_;
            std::string line = s_.substr(start, pos_ - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (pos_ < static_cast<intcs>(s_.size())) ++pos_; // skip '\n'
            return line;
        }

        /** Reads all remaining text from the current position. */
        [[nodiscard]] std::string ReadToEnd() override {
            if (pos_ >= static_cast<intcs>(s_.size())) return "";
            std::string rest = s_.substr(pos_);
            pos_ = static_cast<intcs>(s_.size());
            return rest;
        }
    };

} // namespace System::IO
