/*
 * Sequence: SEQ0058
 * Track: C++
 * MVP: mvp5
 * Change: Provide channel management helpers for the IRC server, including default log broadcast filters.
 * Tests: smoke_cpp_mvp5_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP
#define LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "irc_channel.hpp"

namespace logcrafter::cpp {

class IRCChannelManager {
public:
    struct LogDelivery {
        int client_fd;
        std::string channel;
    };

    IRCChannelManager();

    void reset();
    std::vector<std::string> join_channels(int client_fd, const std::vector<std::string> &channels);
    std::vector<std::string> part_channels(int client_fd, const std::vector<std::string> &channels);
    void remove_client(int client_fd);
    std::vector<LogDelivery> prepare_log_deliveries(const std::string &message) const;

private:
    static std::string sanitize_channel(const std::string &name);
    static bool is_log_channel(const std::string &name);
    static IRCChannel make_default_channel(const std::string &name);
    IRCChannel &find_or_create_channel(const std::string &name);

    std::unordered_map<std::string, IRCChannel> channels_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP
