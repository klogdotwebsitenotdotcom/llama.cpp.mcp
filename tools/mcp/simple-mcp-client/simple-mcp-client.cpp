#include "llama.h"
#include "chat.h"
#include "common.h"
#include "sampling.h"
#include "json.hpp"
#include "mcp_server.h"
#include "mcp_sse_client.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <array>
#include <map>

using json = nlohmann::json;

struct Config {
    // MCP Server Configs
    struct ServerConfig {
        std::string name;
        std::string host;
        int port;
        std::string type;  // "vscode", "llama", "custom"
    };
    std::vector<ServerConfig> servers;
    
    // Client Config
    bool show_instructions = true;
    bool show_details = true;
    std::string history_path;
} config;

// Represents a connected MCP server
struct MCPServer {
    std::string name;
    std::string type;
    std::unique_ptr<mcp::sse_client> client;
    std::vector<mcp::tool> tools;
};

// Global state
std::vector<MCPServer> connected_servers;
std::map<std::string, std::string> tool_to_server;  // Maps tool IDs to server names

static Config parse_config(int argc, char* argv[]) {
    Config config;
    
    // Default VSCode server
    config.servers.push_back({"vscode", "localhost", 8080, "vscode"});
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--add-server") == 0 && i + 3 < argc) {
            config.servers.push_back({
                argv[i+1],  // name
                argv[i+2],  // host
                std::stoi(argv[i+3]),  // port
                argv[i+4]   // type
            });
            i += 4;
        } else if (strcmp(argv[i], "--hide-instructions") == 0) {
            config.show_instructions = false;
        } else if (strcmp(argv[i], "--hide-details") == 0) {
            config.show_details = false;
        }
    }
    return config;
}

// Connect to an MCP server and discover its tools
bool connect_to_server(const Config::ServerConfig& server_config, MCPServer& server) {
    try {
        server.name = server_config.name;
        server.type = server_config.type;
        server.client = std::make_unique<mcp::sse_client>(server_config.host, server_config.port);
        
        bool initialized = server.client->initialize(
            "llama-mcp-client",  // client name
            "0.1.0"             // version
        );
        
        if (!initialized) {
            std::cerr << "Failed to initialize connection to " << server.name << std::endl;
            return false;
        }
        
        // Discover tools
        server.tools = server.client->get_tools();
        
        // Map tools to this server
        for (const auto& tool : server.tools) {
            tool_to_server[tool.name] = server.name;
        }
        
        std::cout << "Connected to " << server.name << " (" << server.tools.size() << " tools)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to " << server.name << ": " << e.what() << std::endl;
        return false;
    }
}

// Display available tools from all servers
void display_tools() {
    std::cout << "\nAvailable Tools:\n";
    for (const auto& server : connected_servers) {
        std::cout << "\n" << server.name << " (" << server.type << "):\n";
        for (const auto& tool : server.tools) {
            std::cout << "  - " << tool.name << ": " << tool.description << "\n";
        }
    }
    std::cout << std::endl;
}

// Find server that owns a tool
MCPServer* find_server_for_tool(const std::string& tool_name) {
    auto it = tool_to_server.find(tool_name);
    if (it == tool_to_server.end()) {
        return nullptr;
    }
    
    for (auto& server : connected_servers) {
        if (server.name == it->second) {
            return &server;
        }
    }
    return nullptr;
}

// Execute a tool on its server
bool execute_tool(const std::string& tool_name, const json& args) {
    auto* server = find_server_for_tool(tool_name);
    if (!server) {
        std::cerr << "Tool " << tool_name << " not found" << std::endl;
        return false;
    }
    
    try {
        json result = server->client->call_tool(tool_name, args);
        std::cout << "\nResult from " << server->name << ":\n" << result.dump(2) << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error executing tool: " << e.what() << std::endl;
        return false;
    }
}

// Interactive TUI mode
void run_interactive_mode() {
    std::cout << "\nMCP Client Interactive Mode\n";
    if (config.show_instructions) {
        std::cout << "\nInstructions:"
                  << "\n- Type 'tools' to list available tools"
                  << "\n- Type 'tool <name> <args_json>' to execute a tool"
                  << "\n- Type 'servers' to list connected servers"
                  << "\n- Type 'quit' to exit"
                  << "\n";
    }
    
    std::string line;
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, line);
        
        if (line == "quit" || line == "exit") {
            break;
        } else if (line == "tools") {
            display_tools();
        } else if (line == "servers") {
            std::cout << "\nConnected Servers:\n";
            for (const auto& server : connected_servers) {
                std::cout << "- " << server.name << " (" << server.type << ")\n";
            }
        } else if (line.substr(0, 5) == "tool ") {
            // Parse tool name and args
            std::string rest = line.substr(5);
            size_t space = rest.find(' ');
            if (space == std::string::npos) {
                std::cerr << "Usage: tool <name> <args_json>" << std::endl;
                continue;
            }
            
            std::string tool_name = rest.substr(0, space);
            std::string args_str = rest.substr(space + 1);
            
            try {
                json args = json::parse(args_str);
                execute_tool(tool_name, args);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing args: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command. Type 'tools' for available tools." << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    config = parse_config(argc, argv);
    
    // Connect to configured servers
    for (const auto& server_config : config.servers) {
        MCPServer server;
        if (connect_to_server(server_config, server)) {
            connected_servers.push_back(std::move(server));
        }
    }
    
    if (connected_servers.empty()) {
        std::cerr << "No servers connected. Exiting." << std::endl;
        return 1;
    }
    
    // Show initial tool list
    display_tools();
    
    // Enter interactive mode
    run_interactive_mode();
    
    return 0;
} 