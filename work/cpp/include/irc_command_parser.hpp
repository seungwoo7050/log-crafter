/*
 * Sequence: SEQ0060
 * Track: C++
 * MVP: mvp5
 * Change: Declare a minimal IRC command parser for newline-delimited client input.
 * Tests: smoke_cpp_mvp5_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_COMMAND_PARSER_HPP
#define LOGCRAFTER_CPP_IRC_COMMAND_PARSER_HPP

#include <string>
#include <vector>

namespace logcrafter::cpp {

struct IRCCommand {
    std::string verb;
    std::vector<std::string> params;
};

class IRCCommandParser {
public:
    static bool parse(const std::string &line, IRCCommand &command);
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_COMMAND_PARSER_HPP
