/*
 * Sequence: SEQ0059
 * Track: C++
 * MVP: mvp5
 * Change: Manage IRC channels, reserved log streams, and routing decisions for new log entries.
 * Tests: smoke_cpp_mvp5_irc
 */
#include "irc_channel_manager.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

namespace logcrafter::cpp {
namespace {

bool contains_token(const std::string &haystack, const std::string &token) {
    if (token.empty()) {
        return true;
    }
    auto it = std::search(haystack.begin(), haystack.end(), token.begin(), token.end(),
                          [](unsigned char lhs, unsigned char rhs) { return std::tolower(lhs) == rhs; });
    return it != haystack.end();
}

std::string lowercase(const std::string &value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result;
}

} // namespace

IRCChannelManager::IRCChannelManager() : channels_() { reset(); }

void IRCChannelManager::reset() {
    channels_.clear();
    const std::vector<std::string> defaults = {
        "#logs-all",
        "#logs-error",
        "#logs-warning",
        "#logs-info",
        "#logs-debug",
    };
    for (const std::string &name : defaults) {
        channels_.emplace(name, make_default_channel(name));
    }
}

std::vector<std::string> IRCChannelManager::join_channels(int client_fd, const std::vector<std::string> &channels) {
    std::vector<std::string> joined;
    std::unordered_set<std::string> seen;
    for (const std::string &raw : channels) {
        const std::string sanitized = sanitize_channel(raw);
        if (sanitized.empty()) {
            continue;
        }
        if (!seen.insert(sanitized).second) {
            continue;
        }
        IRCChannel &channel = find_or_create_channel(sanitized);
        channel.add_member(client_fd);
        joined.push_back(channel.name());
    }
    return joined;
}

std::vector<std::string> IRCChannelManager::part_channels(int client_fd, const std::vector<std::string> &channels) {
    std::vector<std::string> parted;
    std::unordered_set<std::string> seen;
    for (const std::string &raw : channels) {
        const std::string sanitized = sanitize_channel(raw);
        if (sanitized.empty()) {
            continue;
        }
        if (!seen.insert(sanitized).second) {
            continue;
        }
        auto it = channels_.find(sanitized);
        if (it == channels_.end()) {
            continue;
        }
        IRCChannel &channel = it->second;
        if (channel.has_member(client_fd)) {
            channel.remove_member(client_fd);
            parted.push_back(channel.name());
            if (!is_log_channel(sanitized) && channel.members().empty()) {
                channels_.erase(it);
            }
        }
    }
    return parted;
}

void IRCChannelManager::remove_client(int client_fd) {
    std::vector<std::string> to_erase;
    for (auto &entry : channels_) {
        IRCChannel &channel = entry.second;
        if (channel.has_member(client_fd)) {
            channel.remove_member(client_fd);
        }
        if (!is_log_channel(channel.name()) && channel.members().empty()) {
            to_erase.push_back(channel.name());
        }
    }
    for (const std::string &name : to_erase) {
        channels_.erase(name);
    }
}

std::vector<IRCChannelManager::LogDelivery> IRCChannelManager::prepare_log_deliveries(const std::string &message) const {
    std::vector<LogDelivery> deliveries;
    for (const auto &entry : channels_) {
        const IRCChannel &channel = entry.second;
        if (!channel.broadcasts_logs() || !channel.should_broadcast(message)) {
            continue;
        }
        for (int fd : channel.members()) {
            deliveries.push_back({fd, channel.name()});
        }
    }
    return deliveries;
}

std::string IRCChannelManager::sanitize_channel(const std::string &name) {
    std::string trimmed;
    trimmed.reserve(name.size());
    for (unsigned char ch : name) {
        if (std::isspace(ch)) {
            continue;
        }
        trimmed.push_back(static_cast<char>(ch));
    }
    if (trimmed.empty()) {
        return {};
    }
    if (trimmed.front() != '#') {
        trimmed.insert(trimmed.begin(), '#');
    }
    if (trimmed.size() > 32) {
        trimmed.resize(32);
    }
    return lowercase(trimmed);
}

bool IRCChannelManager::is_log_channel(const std::string &name) {
    return name.rfind("#logs-", 0) == 0;
}

IRCChannel IRCChannelManager::make_default_channel(const std::string &name) {
    IRCChannel channel(name, "LogCrafter log stream", true);
    const std::string lowered = lowercase(name);
    if (lowered == "#logs-error") {
        channel.set_filter([](const std::string &message) { return contains_token(message, "error"); });
    } else if (lowered == "#logs-warning") {
        channel.set_filter([](const std::string &message) { return contains_token(message, "warn"); });
    } else if (lowered == "#logs-info") {
        channel.set_filter([](const std::string &message) { return contains_token(message, "info"); });
    } else if (lowered == "#logs-debug") {
        channel.set_filter([](const std::string &message) { return contains_token(message, "debug"); });
    }
    return channel;
}

IRCChannel &IRCChannelManager::find_or_create_channel(const std::string &name) {
    const std::string lowered = lowercase(name);
    auto it = channels_.find(lowered);
    if (it != channels_.end()) {
        return it->second;
    }
    if (is_log_channel(lowered)) {
        auto inserted = channels_.emplace(lowered, make_default_channel(lowered));
        return inserted.first->second;
    }
    IRCChannel channel(lowered, "LogCrafter discussion", false);
    auto inserted = channels_.emplace(lowered, std::move(channel));
    return inserted.first->second;
}

} // namespace logcrafter::cpp
