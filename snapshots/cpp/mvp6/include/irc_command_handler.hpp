/*
 * Sequence: SEQ0065
 * Track: C++
 * MVP: mvp6
 * Change: Declare helpers for handling LogCrafter-specific IRC commands such as !query and !logstream.
 * Tests: smoke_cpp_mvp6_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_COMMAND_HANDLER_HPP
#define LOGCRAFTER_CPP_IRC_COMMAND_HANDLER_HPP

#include <functional>
#include <string>
#include <vector>

#include "irc_channel_manager.hpp"
#include "log_buffer.hpp"
#include "persistence.hpp"

namespace logcrafter::cpp {

struct IRCCommand;

struct IRCCommandReply {
    enum class Type {
        Notice,
        Privmsg,
    };

    Type type;
    std::string target;
    std::string text;
};

struct IRCCommandResult {
    bool handled = false;
    std::vector<IRCCommandReply> replies;
    std::vector<std::string> join_channels;
    std::vector<std::string> part_channels;
};

class IRCCommandHandler {
public:
    using StatsCallback = std::function<std::string()>;

    IRCCommandHandler(LogBuffer &buffer, IRCChannelManager &channels);

    void set_stats_callback(StatsCallback cb);

    IRCCommandResult handle_privmsg(int client_fd,
                                    const std::string &nickname,
                                    const std::string &target,
                                    const std::string &message) const;

private:
    IRCCommandResult handle_query_command(int client_fd,
                                          const std::string &nickname,
                                          const std::string &arguments) const;
    IRCCommandResult handle_logstream_command(int client_fd,
                                              const std::string &nickname,
                                              const std::string &arguments) const;
    IRCCommandResult handle_logfilter_command(int client_fd,
                                              const std::string &nickname,
                                              const std::string &arguments) const;
    IRCCommandResult handle_logstats_command(const std::string &nickname) const;
    IRCCommandResult handle_help_command(const std::string &nickname) const;

    static std::string trim(const std::string &value);
    static std::vector<std::string> split_list(const std::string &value);

    LogBuffer &buffer_;
    IRCChannelManager &channels_;
    StatsCallback stats_callback_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_COMMAND_HANDLER_HPP

