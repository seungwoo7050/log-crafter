#pragma once

#include <memory>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "IRCCommandParser.h"

class IRCServer;
class IRCClient;
class IRCChannel;
class LogBuffer;

// [SEQUENCE: 556] IRC Command Handler - processes parsed IRC commands
class IRCCommandHandler {
public:
    // [SEQUENCE: 557] Command handler function type
    using CommandHandler = std::function<void(std::shared_ptr<IRCClient>, 
                                              const IRCCommandParser::IRCCommand&)>;
    
    // [SEQUENCE: 558] Constructor with server reference
    explicit IRCCommandHandler(IRCServer* server);
    ~IRCCommandHandler();
    
    // [SEQUENCE: 559] Main command dispatch
    void handleCommand(std::shared_ptr<IRCClient> client, 
                      const IRCCommandParser::IRCCommand& cmd);
    
    // [SEQUENCE: 560] Register custom command handlers
    void registerHandler(const std::string& command, CommandHandler handler);
    void unregisterHandler(const std::string& command);
    
private:
    // [SEQUENCE: 561] Core IRC command handlers
    void handleNICK(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleUSER(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handlePASS(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleJOIN(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handlePART(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handlePRIVMSG(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleNOTICE(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleQUIT(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handlePING(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handlePONG(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    
    // [SEQUENCE: 562] Channel management commands
    void handleTOPIC(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleNAMES(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleLIST(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleKICK(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleMODE(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleWHO(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleWHOIS(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    
    // [SEQUENCE: 563] Server query commands
    void handleMOTD(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleVERSION(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleTIME(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleINFO(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    
    // [SEQUENCE: 564] LogCrafter-specific commands
    void handleLOGQUERY(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleLOGFILTER(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleLOGSTREAM(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    void handleLOGSTATS(std::shared_ptr<IRCClient> client, const IRCCommandParser::IRCCommand& cmd);
    
    // [SEQUENCE: 565] Helper methods
    void sendWelcome(std::shared_ptr<IRCClient> client);
    void sendMOTD(std::shared_ptr<IRCClient> client);
    void checkAuthentication(std::shared_ptr<IRCClient> client);
    bool isNicknameAvailable(const std::string& nickname);
    void notifyChannelJoin(std::shared_ptr<IRCChannel> channel, std::shared_ptr<IRCClient> client);
    void notifyChannelPart(std::shared_ptr<IRCChannel> channel, std::shared_ptr<IRCClient> client, const std::string& reason);
    
    // [SEQUENCE: 566] LogCrafter integration helpers
    void processLogQuery(std::shared_ptr<IRCClient> client, const std::string& channel, const std::string& query);
    void setupLogStreamChannel(const std::string& channelName, const std::string& filter);
    std::string formatLogEntryForIRC(const LogEntry& entry);
    
    // [SEQUENCE: 567] Server reference and handler map
    IRCServer* server_;
    std::map<std::string, CommandHandler> handlers_;
    
    // [SEQUENCE: 568] Default log streaming channels
    void initializeDefaultLogChannels();
    void createLogChannel(const std::string& name, const std::string& level);
};