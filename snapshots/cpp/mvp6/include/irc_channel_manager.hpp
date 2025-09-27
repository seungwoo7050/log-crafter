/*
 * Sequence: SEQ0069
 * Track: C++
 * MVP: mvp6
 * Change: Expose channel statistics and custom filter helpers for IRC command integrations.
 * Tests: smoke_cpp_mvp6_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP
#define LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP

#include <functional>
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

    struct ChannelStats {
        std::string name;
        std::size_t members;
        std::size_t broadcasts;
        bool broadcasts_logs;
    };

    IRCChannelManager();

    void reset();
    std::vector<std::string> join_channels(int client_fd, const std::vector<std::string> &channels);
    std::vector<std::string> part_channels(int client_fd, const std::vector<std::string> &channels);
    void remove_client(int client_fd);
    std::vector<LogDelivery> prepare_log_deliveries(const std::string &message);
    std::vector<ChannelStats> stats() const;
    void ensure_filter_channel(const std::string &channel_name,
                               const std::string &topic,
                               std::function<bool(const std::string &)> filter);
    std::vector<int> members_for(const std::string &channel) const;
    std::string topic_for(const std::string &channel) const;

private:
    static std::string sanitize_channel(const std::string &name);
    static bool is_log_channel(const std::string &name);
    static IRCChannel make_default_channel(const std::string &name);
    IRCChannel &find_or_create_channel(const std::string &name);

    std::unordered_map<std::string, IRCChannel> channels_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_CHANNEL_MANAGER_HPP
