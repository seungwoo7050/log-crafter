#include "IRCCommandHandler.h"
#include "IRCServer.h"
#include "IRCClient.h"
#include "IRCChannel.h"
#include "IRCChannelManager.h"
#include "IRCClientManager.h"
#include "LogBuffer.h"
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

// [SEQUENCE: 780] Constructor - register command handlers
IRCCommandHandler::IRCCommandHandler(IRCServer* server) : server_(server) {
    // [SEQUENCE: 781] Register core IRC command handlers
    handlers_["NICK"] = [this](auto client, auto cmd) { handleNICK(client, cmd); };
    handlers_["USER"] = [this](auto client, auto cmd) { handleUSER(client, cmd); };
    handlers_["PASS"] = [this](auto client, auto cmd) { handlePASS(client, cmd); };
    handlers_["JOIN"] = [this](auto client, auto cmd) { handleJOIN(client, cmd); };
    handlers_["PART"] = [this](auto client, auto cmd) { handlePART(client, cmd); };
    handlers_["PRIVMSG"] = [this](auto client, auto cmd) { handlePRIVMSG(client, cmd); };
    handlers_["NOTICE"] = [this](auto client, auto cmd) { handleNOTICE(client, cmd); };
    handlers_["QUIT"] = [this](auto client, auto cmd) { handleQUIT(client, cmd); };
    handlers_["PING"] = [this](auto client, auto cmd) { handlePING(client, cmd); };
    handlers_["PONG"] = [this](auto client, auto cmd) { handlePONG(client, cmd); };
    
    // [SEQUENCE: 782] Register channel management handlers
    handlers_["TOPIC"] = [this](auto client, auto cmd) { handleTOPIC(client, cmd); };
    handlers_["NAMES"] = [this](auto client, auto cmd) { handleNAMES(client, cmd); };
    handlers_["LIST"] = [this](auto client, auto cmd) { handleLIST(client, cmd); };
    handlers_["KICK"] = [this](auto client, auto cmd) { handleKICK(client, cmd); };
    handlers_["MODE"] = [this](auto client, auto cmd) { handleMODE(client, cmd); };
    handlers_["WHO"] = [this](auto client, auto cmd) { handleWHO(client, cmd); };
    handlers_["WHOIS"] = [this](auto client, auto cmd) { handleWHOIS(client, cmd); };
    
    // [SEQUENCE: 783] Register server query handlers
    handlers_["MOTD"] = [this](auto client, auto cmd) { handleMOTD(client, cmd); };
    handlers_["VERSION"] = [this](auto client, auto cmd) { handleVERSION(client, cmd); };
    handlers_["TIME"] = [this](auto client, auto cmd) { handleTIME(client, cmd); };
    handlers_["INFO"] = [this](auto client, auto cmd) { handleINFO(client, cmd); };
    
    // [SEQUENCE: 784] Register LogCrafter-specific handlers
    handlers_["LOGQUERY"] = [this](auto client, auto cmd) { handleLOGQUERY(client, cmd); };
    handlers_["LOGFILTER"] = [this](auto client, auto cmd) { handleLOGFILTER(client, cmd); };
    handlers_["LOGSTREAM"] = [this](auto client, auto cmd) { handleLOGSTREAM(client, cmd); };
    handlers_["LOGSTATS"] = [this](auto client, auto cmd) { handleLOGSTATS(client, cmd); };
}

// [SEQUENCE: 785] Destructor
IRCCommandHandler::~IRCCommandHandler() = default;

// [SEQUENCE: 786] Main command dispatch
void IRCCommandHandler::handleCommand(std::shared_ptr<IRCClient> client, 
                                     const IRCCommandParser::IRCCommand& cmd) {
    if (!client || cmd.command.empty()) {
        return;
    }
    
    // [SEQUENCE: 787] Check if client needs to register first
    if (!client->isAuthenticated() && 
        cmd.command != "NICK" && cmd.command != "USER" && 
        cmd.command != "PASS" && cmd.command != "QUIT") {
        client->sendErrorReply(IRCCommandParser::ERR_NOTREGISTERED, 
                              ":You have not registered");
        return;
    }
    
    // [SEQUENCE: 788] Find and execute handler
    auto it = handlers_.find(cmd.command);
    if (it != handlers_.end()) {
        it->second(client, cmd);
    } else {
        client->sendErrorReply(IRCCommandParser::ERR_UNKNOWNCOMMAND,
                              cmd.command + " :Unknown command");
    }
}

// [SEQUENCE: 789] Handle NICK command
void IRCCommandHandler::handleNICK(std::shared_ptr<IRCClient> client, 
                                   const IRCCommandParser::IRCCommand& cmd) {
    if (cmd.params.empty()) {
        client->sendErrorReply(IRCCommandParser::ERR_NONICKNAMEGIVEN,
                              ":No nickname given");
        return;
    }
    
    std::string newNick = cmd.getParam(0);
    
    // [SEQUENCE: 790] Validate nickname
    if (!IRCCommandParser::isValidNickname(newNick)) {
        client->sendErrorReply(IRCCommandParser::ERR_ERRONEUSNICKNAME,
                              newNick + " :Erroneous nickname");
        return;
    }
    
    // [SEQUENCE: 791] Check if nickname is in use
    if (!isNicknameAvailable(newNick)) {
        client->sendErrorReply(IRCCommandParser::ERR_NICKNAMEINUSE,
                              newNick + " :Nickname is already in use");
        return;
    }
    
    // [SEQUENCE: 792] Process nickname change
    std::string oldNick = client->getNickname();
    auto clientManager = server_->getClientManager();
    
    if (!oldNick.empty()) {
        // [SEQUENCE: 793] Nickname change for existing client
        if (clientManager->changeNickname(oldNick, newNick)) {
            client->setNickname(newNick);
            
            // Notify all channels
            std::string nickChangeMsg = ":" + oldNick + " NICK :" + newNick;
            for (const auto& channelName : client->getChannels()) {
                auto channel = server_->getChannelManager()->getChannel(channelName);
                if (channel) {
                    channel->broadcast(nickChangeMsg);
                }
            }
        }
    } else {
        // [SEQUENCE: 794] Initial nickname set
        client->setNickname(newNick);
        clientManager->registerNickname(client->getFd(), newNick);
        checkAuthentication(client);
    }
}

// [SEQUENCE: 795] Handle USER command
void IRCCommandHandler::handleUSER(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    if (client->getState() == IRCClient::State::AUTHENTICATED) {
        client->sendErrorReply(IRCCommandParser::ERR_ALREADYREGISTRED,
                              ":You may not reregister");
        return;
    }
    
    if (cmd.params.size() < 4) {
        client->sendErrorReply(IRCCommandParser::ERR_NEEDMOREPARAMS,
                              "USER :Not enough parameters");
        return;
    }
    
    // [SEQUENCE: 796] Set user information
    client->setUsername(cmd.getParam(0));
    client->setHostname(cmd.getParam(1)); // Usually ignored in favor of actual connection
    client->setRealname(cmd.trailing.empty() ? cmd.getParam(3) : cmd.trailing);
    
    checkAuthentication(client);
}

// [SEQUENCE: 797] Handle PASS command (optional)
void IRCCommandHandler::handlePASS(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    // For now, we don't require passwords
    // This is where server password checking would go
}

// [SEQUENCE: 798] Handle JOIN command
void IRCCommandHandler::handleJOIN(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    if (cmd.params.empty()) {
        client->sendErrorReply(IRCCommandParser::ERR_NEEDMOREPARAMS,
                              "JOIN :Not enough parameters");
        return;
    }
    
    // [SEQUENCE: 799] Parse channel list and keys
    std::vector<std::string> channels = IRCCommandParser::splitChannels(cmd.getParam(0));
    std::vector<std::string> keys;
    if (cmd.params.size() > 1) {
        keys = IRCCommandParser::splitChannels(cmd.getParam(1));
    }
    
    auto channelManager = server_->getChannelManager();
    
    // [SEQUENCE: 800] Join each channel
    for (size_t i = 0; i < channels.size(); ++i) {
        const std::string& channelName = channels[i];
        std::string key = (i < keys.size()) ? keys[i] : "";
        
        if (!IRCCommandParser::isValidChannelName(channelName)) {
            client->sendErrorReply(IRCCommandParser::ERR_NOSUCHCHANNEL,
                                  channelName + " :No such channel");
            continue;
        }
        
        // [SEQUENCE: 801] Special handling for log channels
        if (channelName.find("#logs-") == 0 && !channelManager->channelExists(channelName)) {
            client->sendErrorReply(IRCCommandParser::ERR_NOSUCHCHANNEL,
                                  channelName + " :Log channel does not exist");
            continue;
        }
        
        channelManager->joinChannel(client, channelName, key);
    }
}

// [SEQUENCE: 802] Handle PART command
void IRCCommandHandler::handlePART(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    if (cmd.params.empty()) {
        client->sendErrorReply(IRCCommandParser::ERR_NEEDMOREPARAMS,
                              "PART :Not enough parameters");
        return;
    }
    
    std::vector<std::string> channels = IRCCommandParser::splitChannels(cmd.getParam(0));
    std::string reason = cmd.trailing.empty() ? client->getNickname() : cmd.trailing;
    
    auto channelManager = server_->getChannelManager();
    
    for (const auto& channelName : channels) {
        if (!channelManager->channelExists(channelName)) {
            client->sendErrorReply(IRCCommandParser::ERR_NOSUCHCHANNEL,
                                  channelName + " :No such channel");
            continue;
        }
        
        if (!client->isInChannel(channelName)) {
            client->sendErrorReply(IRCCommandParser::ERR_NOTONCHANNEL,
                                  channelName + " :You're not on that channel");
            continue;
        }
        
        channelManager->partChannel(client, channelName, reason);
    }
}

// [SEQUENCE: 803] Handle PRIVMSG command
void IRCCommandHandler::handlePRIVMSG(std::shared_ptr<IRCClient> client,
                                      const IRCCommandParser::IRCCommand& cmd) {
    if (cmd.params.empty()) {
        client->sendErrorReply(IRCCommandParser::ERR_NORECIPIENT,
                              ":No recipient given (PRIVMSG)");
        return;
    }
    
    if (cmd.trailing.empty() && cmd.params.size() < 2) {
        client->sendErrorReply(IRCCommandParser::ERR_NOTEXTTOSEND,
                              ":No text to send");
        return;
    }
    
    std::string target = cmd.getParam(0);
    std::string message = cmd.trailing.empty() ? cmd.getParam(1) : cmd.trailing;
    
    // [SEQUENCE: 804] Check for LogCrafter commands in channel messages
    if (target[0] == '#' && message[0] == '!') {
        processLogQuery(client, target, message);
        return;
    }
    
    // [SEQUENCE: 805] Channel message
    if (target[0] == '#' || target[0] == '&') {
        auto channel = server_->getChannelManager()->getChannel(target);
        if (!channel) {
            client->sendErrorReply(IRCCommandParser::ERR_NOSUCHCHANNEL,
                                  target + " :No such channel");
            return;
        }
        
        if (!channel->hasClient(client->getNickname())) {
            client->sendErrorReply(IRCCommandParser::ERR_CANNOTSENDTOCHAN,
                                  target + " :Cannot send to channel");
            return;
        }
        
        std::string privmsg = IRCCommandParser::formatUserMessage(
            client->getNickname(), client->getUsername(), client->getHostname(),
            "PRIVMSG", target, message
        );
        channel->broadcastExcept(privmsg, client->getNickname());
    } else {
        // [SEQUENCE: 806] Private message
        auto targetClient = server_->getClientManager()->getClientByNickname(target);
        if (!targetClient) {
            client->sendErrorReply(IRCCommandParser::ERR_NOSUCHNICK,
                                  target + " :No such nick/channel");
            return;
        }
        
        std::string privmsg = IRCCommandParser::formatUserMessage(
            client->getNickname(), client->getUsername(), client->getHostname(),
            "PRIVMSG", target, message
        );
        targetClient->sendMessage(privmsg);
    }
}

// [SEQUENCE: 807] Handle NOTICE command
void IRCCommandHandler::handleNOTICE(std::shared_ptr<IRCClient> client,
                                     const IRCCommandParser::IRCCommand& cmd) {
    // Similar to PRIVMSG but no error replies
    if (cmd.params.size() < 2 && cmd.trailing.empty()) {
        return;
    }
    
    std::string target = cmd.getParam(0);
    std::string message = cmd.trailing.empty() ? cmd.getParam(1) : cmd.trailing;
    
    if (target[0] == '#' || target[0] == '&') {
        auto channel = server_->getChannelManager()->getChannel(target);
        if (channel && channel->hasClient(client->getNickname())) {
            std::string notice = IRCCommandParser::formatUserMessage(
                client->getNickname(), client->getUsername(), client->getHostname(),
                "NOTICE", target, message
            );
            channel->broadcastExcept(notice, client->getNickname());
        }
    } else {
        auto targetClient = server_->getClientManager()->getClientByNickname(target);
        if (targetClient) {
            std::string notice = IRCCommandParser::formatUserMessage(
                client->getNickname(), client->getUsername(), client->getHostname(),
                "NOTICE", target, message
            );
            targetClient->sendMessage(notice);
        }
    }
}

// [SEQUENCE: 808] Handle QUIT command
void IRCCommandHandler::handleQUIT(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    std::string quitMessage = cmd.trailing.empty() ? "Client Quit" : cmd.trailing;
    
    // [SEQUENCE: 809] Notify all channels
    std::string quitNotice = ":" + client->getFullIdentifier() + " QUIT :" + quitMessage;
    for (const auto& channelName : client->getChannels()) {
        auto channel = server_->getChannelManager()->getChannel(channelName);
        if (channel) {
            channel->broadcastExcept(quitNotice, client->getNickname());
        }
    }
    
    // [SEQUENCE: 810] Clean up client
    server_->getChannelManager()->partAllChannels(client, quitMessage);
    client->setState(IRCClient::State::DISCONNECTED);
    
    // Server will handle actual disconnection
}

// [SEQUENCE: 811] Handle PING command
void IRCCommandHandler::handlePING(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    std::string param = cmd.params.empty() ? server_->getServerName() : cmd.getParam(0);
    client->sendMessage(":" + server_->getServerName() + " PONG " + 
                       server_->getServerName() + " :" + param);
}

// [SEQUENCE: 812] Handle PONG command
void IRCCommandHandler::handlePONG(std::shared_ptr<IRCClient> client,
                                   const IRCCommandParser::IRCCommand& cmd) {
    // Update activity timestamp
    client->updateLastActivity();
}

// [SEQUENCE: 813] Handle TOPIC command
void IRCCommandHandler::handleTOPIC(std::shared_ptr<IRCClient> client,
                                    const IRCCommandParser::IRCCommand& cmd) {
    if (cmd.params.empty()) {
        client->sendErrorReply(IRCCommandParser::ERR_NEEDMOREPARAMS,
                              "TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = cmd.getParam(0);
    auto channel = server_->getChannelManager()->getChannel(channelName);
    
    if (!channel) {
        client->sendErrorReply(IRCCommandParser::ERR_NOSUCHCHANNEL,
                              channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasClient(client->getNickname())) {
        client->sendErrorReply(IRCCommandParser::ERR_NOTONCHANNEL,
                              channelName + " :You're not on that channel");
        return;
    }
    
    // [SEQUENCE: 814] Get topic
    if (cmd.params.size() == 1 && cmd.trailing.empty()) {
        if (channel->getTopic().empty()) {
            client->sendNumericReply(IRCCommandParser::RPL_NOTOPIC,
                                    channelName + " :No topic is set");
        } else {
            client->sendNumericReply(IRCCommandParser::RPL_TOPIC,
                                    channelName + " :" + channel->getTopic());
        }
        return;
    }
    
    // [SEQUENCE: 815] Set topic
    if (channel->getModes().topicProtected && !channel->isOperator(client->getNickname())) {
        client->sendErrorReply(IRCCommandParser::ERR_CHANOPRIVSNEEDED,
                              channelName + " :You're not channel operator");
        return;
    }
    
    std::string newTopic = cmd.trailing.empty() ? "" : cmd.trailing;
    channel->setTopic(newTopic, client->getNickname());
    
    // Notify all users
    std::string topicMsg = ":" + client->getFullIdentifier() + " TOPIC " + 
                          channelName + " :" + newTopic;
    channel->broadcast(topicMsg);
}

// [SEQUENCE: 816] Continue with remaining handlers...
void IRCCommandHandler::handleNAMES(std::shared_ptr<IRCClient> client,
                                    const IRCCommandParser::IRCCommand& cmd) {
    // Implementation continues...
}

// [SEQUENCE: 817] Send welcome messages
void IRCCommandHandler::sendWelcome(std::shared_ptr<IRCClient> client) {
    std::string nick = client->getNickname();
    std::string serverName = server_->getServerName();
    
    // [SEQUENCE: 818] Send standard welcome sequence
    client->sendNumericReply(IRCCommandParser::RPL_WELCOME,
        ":Welcome to the LogCrafter IRC Network " + client->getFullIdentifier());
    
    client->sendNumericReply(IRCCommandParser::RPL_YOURHOST,
        ":Your host is " + serverName + ", running version " + server_->getServerVersion());
    
    client->sendNumericReply(IRCCommandParser::RPL_CREATED,
        ":This server was created " + server_->getServerCreated());
    
    client->sendNumericReply(IRCCommandParser::RPL_MYINFO,
        serverName + " " + server_->getServerVersion() + " o o");
    
    // [SEQUENCE: 819] Send MOTD
    sendMOTD(client);
    
    // [SEQUENCE: 820] Initialize default log channels if needed
    initializeDefaultLogChannels();
}

// [SEQUENCE: 821] Send MOTD
void IRCCommandHandler::sendMOTD(std::shared_ptr<IRCClient> client) {
    client->sendNumericReply(IRCCommandParser::RPL_MOTDSTART,
        ":- " + server_->getServerName() + " Message of the day - ");
    
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- Welcome to LogCrafter-IRC!");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- This server combines IRC with real-time log monitoring.");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- ");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- Available log channels:");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":-   #logs-all     - All log messages");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":-   #logs-error   - Error level logs");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":-   #logs-warning - Warning level logs");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":-   #logs-info    - Info level logs");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":-   #logs-debug   - Debug level logs");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- ");
    client->sendNumericReply(IRCCommandParser::RPL_MOTD,
        ":- Use !query <query> in channels for log queries");
    
    client->sendNumericReply(IRCCommandParser::RPL_ENDOFMOTD,
        ":End of /MOTD command");
}

// [SEQUENCE: 822] Check if client is fully authenticated
void IRCCommandHandler::checkAuthentication(std::shared_ptr<IRCClient> client) {
    if (!client->getNickname().empty() && !client->getUsername().empty()) {
        client->setState(IRCClient::State::AUTHENTICATED);
        sendWelcome(client);
    }
}

// [SEQUENCE: 823] Check nickname availability
bool IRCCommandHandler::isNicknameAvailable(const std::string& nickname) {
    return server_->getClientManager()->isNicknameAvailable(nickname);
}

// [SEQUENCE: 824] Initialize default log channels
void IRCCommandHandler::initializeDefaultLogChannels() {
    static bool initialized = false;
    if (!initialized) {
        server_->getChannelManager()->initializeLogChannels();
        
        // [SEQUENCE: 825] Set up log distribution callback
        auto logBuffer = server_->getLogBuffer();
        if (logBuffer) {
            auto channelManager = server_->getChannelManager();
            channelManager->registerLogCallback([channelManager](const LogEntry& entry) {
                channelManager->distributeLogEntry(entry);
            });
        }
        
        initialized = true;
    }
}

// [SEQUENCE: 826] Process log query commands
void IRCCommandHandler::processLogQuery(std::shared_ptr<IRCClient> client,
                                       const std::string& channel,
                                       const std::string& query) {
    // Parse command (e.g., "!query COUNT level:ERROR")
    std::istringstream iss(query);
    std::string command;
    iss >> command;
    
    if (command == "!query") {
        // Extract query parameters
        std::string queryStr;
        std::getline(iss, queryStr);
        
        // TODO: Implement actual query processing using LogBuffer
        std::string response = "Query processing not yet implemented: " + queryStr;
        
        std::string msg = IRCCommandParser::formatUserMessage(
            "LogBot", "log", "system", "PRIVMSG", channel, response
        );
        
        auto chan = server_->getChannelManager()->getChannel(channel);
        if (chan) {
            chan->broadcast(msg);
        }
    }
}

// [SEQUENCE: 827] Register custom handler
void IRCCommandHandler::registerHandler(const std::string& command, CommandHandler handler) {
    handlers_[IRCCommandParser::toUpper(command)] = handler;
}

// [SEQUENCE: 828] Unregister handler
void IRCCommandHandler::unregisterHandler(const std::string& command) {
    handlers_.erase(IRCCommandParser::toUpper(command));
}