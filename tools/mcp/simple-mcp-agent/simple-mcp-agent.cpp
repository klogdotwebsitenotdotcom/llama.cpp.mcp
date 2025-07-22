//
// Calculator version of the MCP agent that:
// 1. Uses local Llama model instead of external API
// 2. Implements the same calculator tool as agent_example.cpp
// 3. Maintains identical server/client architecture
//
// Usage:
// Required:
//   -m <path>       Path to the GGUF model file
// Optional:
//   --port <n>      Server port (default: 8889)
//

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
#include <regex>
#include <sstream>

using json = nlohmann::json;

// Forward declarations
std::string clean_llm_response(const std::string& response);

class CalculatorHandler {
public:
    static mcp::json handle(const mcp::json& params, const std::string& /* session_id */) {
        if (!params.contains("expression")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'expression' parameter");
        }

        std::string expr = clean_llm_response(params["expression"]);
        if (expr.empty()) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Empty expression");
        }

        try {
            double result = evaluate_expression(expr);
            return {
                {
                    {"type", "text"},
                    {"text", std::to_string(result)}
                }
            };
        } catch (const std::exception& e) {
            throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
        }
    }

private:
    static double evaluate_expression(const std::string& expr) {
        std::istringstream iss(expr);
        double a, b;
        char op;
        
        iss >> a >> op >> b;
        
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/':
                if (b == 0) throw std::runtime_error("Division by zero");
                return a / b;
            default:
                throw std::runtime_error("Invalid operator");
        }
    }
};

class ShellCommandHandler {
public:
    static mcp::json handle(const mcp::json& params, const std::string& /* session_id */) {
        if (!params.contains("command")) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'command' parameter");
        }

        std::string cmd = clean_llm_response(params["command"]);
        if (cmd.empty()) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Empty command");
        }

        if (!is_command_safe(cmd)) {
            throw mcp::mcp_exception(mcp::error_code::invalid_params, "Command not allowed for security reasons");
        }

        try {
            std::string result = execute_command(cmd);
            return {
                {
                    {"type", "text"},
                    {"text", result}
                }
            };
        } catch (const std::exception& e) {
            throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
        }
    }

private:
    static bool is_command_safe(const std::string& cmd) {
        std::vector<std::string> blocked = {
            "rm", "sudo", "su", ">", ">>", "|",
            "mv", "cp", "chmod", "chown", "&"
        };
        
        for (const auto& blocked_cmd : blocked) {
            if (cmd.find(blocked_cmd) != std::string::npos) {
                return false;
            }
        }
        
        std::vector<std::string> allowed = {
            "ls", "pwd", "echo", "cat",
            "date", "whoami", "uname"
        };
        
        for (const auto& allowed_cmd : allowed) {
            if (cmd.substr(0, allowed_cmd.length()) == allowed_cmd) {
                return true;
            }
        }
        
        return false;
    }
    
    static std::string execute_command(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to execute command");
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        
        pclose(pipe);
        return result;
    }
};

// Clean LLM response markers
std::string clean_llm_response(const std::string& response) {
    std::string result = response;
    
    std::vector<std::string> markers = {
        "<|im_start|>", "<|im_end|>",
        "<|assistant|>", "<|user|>",
        "assistant\n", "user\n"
    };
    
    for (const auto& marker : markers) {
        size_t pos;
        while ((pos = result.find(marker)) != std::string::npos) {
            result.erase(pos, marker.length());
        }
    }
    
    // Trim whitespace
    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \n\r\t"));
        s.erase(s.find_last_not_of(" \n\r\t") + 1);
    };
    trim(result);
    
    return result;
}

struct Config {
    std::string model_path;
    int port = 8889;
    bool confirm_commands = false;
};

static Config parse_config(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.model_path = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--confirm") == 0) {
            config.confirm_commands = true;
        }
    }
    return config;
}

// UTF-8 input handling - identical to agent_example.cpp
static bool readline_utf8(std::string & line, bool multiline_input) {
#if defined(_WIN32)
    std::wstring wline;
    if (!std::getline(std::wcin, wline)) {
        line.clear();
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        return false;
    }
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wline[0], (int)wline.size(), NULL, 0, NULL, NULL);
    line.resize(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, &wline[0], (int)wline.size(), &line[0], size_needed, NULL, NULL);
#else
    if (!std::getline(std::cin, line)) {
        line.clear();
        return false;
    }
#endif
    if (!line.empty()) {
        char last = line.back();
        if (last == '/') {
            line.pop_back();
            return false;
        }
        if (last == '\\') {
            line.pop_back();
            multiline_input = !multiline_input;
        }
    }
    return multiline_input;
}

// Message display - identical to agent_example.cpp
static void display_message(const mcp::json& message) {
    mcp::json content = message.value("content", mcp::json::array());
    mcp::json tool_calls = message.value("tool_calls", mcp::json::array());

    std::string content_to_display;
    if (content.is_string()) {
        content_to_display = content.get<std::string>();
    } else if (content.is_array()) {
        for (const auto& item : content) {
            if (!item.is_object()) {
                throw std::invalid_argument("Invalid content item type");
            }
            if (item["type"] == "text") {
                content_to_display += item["text"].get<std::string>();
            }
        }
    }

    if (!tool_calls.empty()) {
        content_to_display += "\n\nTool calls:\n";
        for (const auto& tool_call : tool_calls) {
            content_to_display += "- " + tool_call["function"]["name"].get<std::string>() + "\n";
        }
    }

    std::cout << content_to_display << "\n\n";
}

int main(int argc, char ** argv) {
    Config config = parse_config(argc, argv);
    if (config.model_path.empty()) {
        std::cerr << "Model path required (-m)\n";
        return 1;
    }

    // Initialize Llama model
    ggml_backend_load_all();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99; // Default to 99 for simplicity
    llama_model * model = llama_model_load_from_file(config.model_path.c_str(), model_params);
    if (!model) {
        std::cerr << "Failed to load model\n";
        return 1;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048; // Default to 2048
    ctx_params.n_batch = 512; // Default to 512
    llama_context * ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "Failed to create context\n";
        return 1;
    }

    // Create server with calculator and shell tools
    mcp::server server("localhost", config.port);
    server.set_server_info("MCPAgent", "0.1.0");
    mcp::json capabilities = {
        {"tools", mcp::json::object()}
    };
    server.set_capabilities(capabilities);

    // Register calculator tool
    mcp::tool calc_tool = mcp::tool_builder("calculator")
        .with_description("Perform basic calculations")
        .with_string_param("expression", "The calculation to perform (e.g., '2 + 2')")
        .build();
    server.register_tool(calc_tool, CalculatorHandler::handle);

    // Register shell command tool
    mcp::tool shell_tool = mcp::tool_builder("shell_command")
        .with_description("Execute basic shell commands")
        .with_string_param("command", "The shell command to execute")
        .build();
    server.register_tool(shell_tool, ShellCommandHandler::handle);

    // Start server
    server.start(false);  // Non-blocking mode

    // Create client
    mcp::sse_client client("localhost", config.port);
    client.set_timeout(10);

    bool initialized = client.initialize("MCPClient", "0.1.0");
    if (!initialized) {
        std::cerr << "Failed to initialize connection to server\n";
        return 1;
    }

    // Get available tools
    std::vector<common_chat_tool> llm_tools;
    {
        auto tools = client.get_tools();
        std::cout << "\nAvailable tools:\n";
        for (const auto& tool : tools) {
            std::cout << "- " << tool.name << ": " << tool.description << "\n";
            llm_tools.push_back(common_chat_tool{
                tool.name,
                tool.description,
                tool.parameters_schema.dump()
            });
        }
        std::cout << std::endl;
    }

    // Initialize chat
    common_chat_templates_ptr chat_templates = common_chat_templates_init(nullptr, "");
    std::vector<common_chat_msg> messages = {
        {
            "system",
            "You are a helpful assistant that can perform calculations and execute basic shell commands.",
            {}, {}, "", "", ""
        }
    };

    // Main loop
    std::string line;
    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, line)) break;

        messages.push_back({
            "user",
            line,
            {}, {}, "", "", ""
        });

        common_chat_templates_inputs inputs;
        inputs.messages = messages;
        inputs.tools = llm_tools;
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_AUTO;
        inputs.add_generation_prompt = true;
        inputs.use_jinja = true;

        auto chat_params = common_chat_templates_apply(chat_templates.get(), inputs);
        std::cout << chat_params.prompt << std::endl;
    }

    llama_free(ctx);
    llama_model_free(model);
    return 0;
} 