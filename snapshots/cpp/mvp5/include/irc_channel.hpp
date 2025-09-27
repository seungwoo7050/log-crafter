/*
 * Sequence: SEQ0056
 * Track: C++
 * MVP: mvp5
 * Change: Define the IRC channel container used to track members and log broadcast filters.
 * Tests: smoke_cpp_mvp5_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_CHANNEL_HPP
#define LOGCRAFTER_CPP_IRC_CHANNEL_HPP

#include <functional>
#include <string>
#include <unordered_set>

namespace logcrafter::cpp {

class IRCChannel {
public:
    IRCChannel();
    IRCChannel(std::string name, std::string topic, bool broadcasts_logs);

    const std::string &name() const;
    const std::string &topic() const;
    void set_topic(const std::string &topic);

    void set_filter(std::function<bool(const std::string &)> filter);
    bool broadcasts_logs() const;
    void set_broadcasts_logs(bool value);
    bool should_broadcast(const std::string &message) const;

    void add_member(int client_fd);
    void remove_member(int client_fd);
    bool has_member(int client_fd) const;
    const std::unordered_set<int> &members() const;

private:
    std::string name_;
    std::string topic_;
    bool broadcasts_logs_;
    std::function<bool(const std::string &)> filter_;
    std::unordered_set<int> members_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_CHANNEL_HPP
