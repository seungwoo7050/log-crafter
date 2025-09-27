/*
 * Sequence: SEQ0055
 * Track: C++
 * MVP: mvp5
 * Change: Declare the IRC server facade that accepts clients and distributes buffered logs to channels.
 * Tests: smoke_cpp_mvp5_irc
 */
#ifndef LOGCRAFTER_CPP_IRC_SERVER_HPP
#define LOGCRAFTER_CPP_IRC_SERVER_HPP

#include <atomic>
#include <cstddef>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "irc_channel_manager.hpp"
#include "irc_command_parser.hpp"

namespace logcrafter::cpp {

class IRCServer {
public:
    IRCServer();
    ~IRCServer();

    int start(int port);
    void request_stop();
    void shutdown();

    void publish_log(const std::string &message, std::time_t timestamp);
    std::size_t active_clients() const;

private:
    struct IRCClient {
        int fd;
        bool has_nick;
        bool has_user;
        bool registered;
        std::string nickname;
        std::string username;
        std::string recv_buffer;
    };

    struct PendingSend {
        int fd;
        std::string line;
    };

    void run_loop();
    void handle_accept();
    bool handle_client_input(int client_fd);
    std::vector<std::string> drain_client_lines(IRCClient &client);
    std::vector<PendingSend> process_command(int client_fd, const IRCCommand &command, bool &client_closed);
    std::vector<PendingSend> register_client(IRCClient &client);
    std::vector<PendingSend> handle_join(IRCClient &client, const IRCCommand &command);
    std::vector<PendingSend> handle_part(IRCClient &client, const IRCCommand &command);
    PendingSend make_notice(const IRCClient &client, const std::string &message) const;
    PendingSend make_unknown_command(const IRCClient &client, const std::string &command) const;
    void close_client_locked(int client_fd);
    void send_lines(const std::vector<PendingSend> &sends);
    void set_socket_nonblocking(int fd);
    static std::string format_privmsg(const std::string &server_name,
                                      const std::string &channel,
                                      const std::string &message,
                                      std::time_t timestamp);

    std::string server_name_;
    int listen_fd_;
    std::atomic<bool> running_;
    std::thread worker_;

    mutable std::mutex mutex_;
    std::unordered_map<int, IRCClient> clients_;
    IRCChannelManager channel_manager_;
    std::atomic<std::size_t> active_clients_;
};

} // namespace logcrafter::cpp

#endif // LOGCRAFTER_CPP_IRC_SERVER_HPP
