/*
 * Sequence: SEQ0068
 * Track: C++
 * MVP: mvp6
 * Change: Track broadcast counts and support dynamic filter configuration for custom channels.
 * Tests: smoke_cpp_mvp6_irc
 */
#include "irc_channel.hpp"

#include <utility>

namespace logcrafter::cpp {

IRCChannel::IRCChannel()
    : name_(), topic_(), broadcasts_logs_(false), filter_(), members_(), broadcast_count_(0) {}

IRCChannel::IRCChannel(std::string name, std::string topic, bool broadcasts_logs)
    : name_(std::move(name)),
      topic_(std::move(topic)),
      broadcasts_logs_(broadcasts_logs),
      filter_(),
      members_(),
      broadcast_count_(0) {}

const std::string &IRCChannel::name() const { return name_; }

const std::string &IRCChannel::topic() const { return topic_; }

void IRCChannel::set_topic(const std::string &topic) { topic_ = topic; }

void IRCChannel::set_filter(std::function<bool(const std::string &)> filter) { filter_ = std::move(filter); }

bool IRCChannel::broadcasts_logs() const { return broadcasts_logs_; }

void IRCChannel::set_broadcasts_logs(bool value) { broadcasts_logs_ = value; }

bool IRCChannel::should_broadcast(const std::string &message) const {
    if (!broadcasts_logs_) {
        return false;
    }
    if (!filter_) {
        return true;
    }
    return filter_(message);
}

void IRCChannel::record_broadcast() { ++broadcast_count_; }

std::size_t IRCChannel::broadcast_count() const { return broadcast_count_; }

void IRCChannel::add_member(int client_fd) { members_.insert(client_fd); }

void IRCChannel::remove_member(int client_fd) { members_.erase(client_fd); }

bool IRCChannel::has_member(int client_fd) const { return members_.find(client_fd) != members_.end(); }

const std::unordered_set<int> &IRCChannel::members() const { return members_; }

} // namespace logcrafter::cpp
