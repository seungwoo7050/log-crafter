/*
 * Sequence: SEQ0061
 * Track: C++
 * MVP: mvp5
 * Change: Implement a tolerant IRC line parser supporting prefixes, trailing params, and verb normalization.
 * Tests: smoke_cpp_mvp5_irc
 */
#include "irc_command_parser.hpp"

#include <algorithm>
#include <cctype>

namespace logcrafter::cpp {

bool IRCCommandParser::parse(const std::string &line, IRCCommand &command) {
    command.verb.clear();
    command.params.clear();

    if (line.empty()) {
        return false;
    }

    std::size_t pos = 0;
    if (line[pos] == ':') {
        std::size_t space = line.find(' ');
        if (space == std::string::npos) {
            return false;
        }
        pos = space + 1;
    }

    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }

    if (pos >= line.size()) {
        return false;
    }

    std::size_t space = line.find(' ', pos);
    std::string verb = space == std::string::npos ? line.substr(pos) : line.substr(pos, space - pos);
    std::transform(verb.begin(), verb.end(), verb.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    if (verb.empty()) {
        return false;
    }

    command.verb = verb;

    if (space == std::string::npos) {
        return true;
    }

    pos = space + 1;
    while (pos < line.size()) {
        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
            ++pos;
        }
        if (pos >= line.size()) {
            break;
        }
        if (line[pos] == ':') {
            command.params.emplace_back(line.substr(pos + 1));
            break;
        }
        std::size_t next_space = line.find(' ', pos);
        if (next_space == std::string::npos) {
            command.params.emplace_back(line.substr(pos));
            break;
        }
        command.params.emplace_back(line.substr(pos, next_space - pos));
        pos = next_space + 1;
    }

    return true;
}

} // namespace logcrafter::cpp
