//
// This is a simplified version of the MCP agent that:
// 1. Uses local Llama model instead of external API (main difference from agent_example.cpp)
// 2. Maintains the same server/client architecture for tool execution
// 3. Implements a basic shell command tool (vs calculator in agent_example.cpp)
//
// Key architectural differences from agent_example.cpp:
// - Local model inference vs HTTP requests to LLM API
// - Simpler configuration (just model path and server port)
// - Single tool implementation vs extensible tool system
// - Direct shell command execution vs abstract tool interface
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

using json = nlohmann::json;

// Configuration struct - simpler than agent_example.cpp
// agent_example.cpp has more LLM-specific config (API keys, endpoints, etc.)
// while we only need model and server settings
struct Config {
    // Model Config
    std::string model_path;
    int n_gpu_layers = 99;
    int n_ctx = 2048;
    int n_batch = 512;
    std::string system_prompt = "You are a helpful assistant that can execute shell commands. When the user asks for something that requires a command, generate and execute the appropriate shell command. Be careful and only execute safe commands.";

    // Server Config - kept similar to agent_example.cpp
    int port = 8889;
    bool confirm_commands = false;
} config;

// Simplified config parser compared to agent_example.cpp
// We only need model path and basic server settings
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

// Tool handler - similar structure to agent_example.cpp's calculator_handler
// Both follow the mcp::json input/output format for tool execution
static mcp::json shell_command_handler(const mcp::json& params, const std::string& /* session_id */) {
    if (!params.contains("command")) {
        throw mcp::mcp_exception(mcp::error_code::invalid_params, "Missing 'command' parameter");
    }

    std::string command = params["command"];
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(command.c_str(), "r"), pclose);
    
    if (!pipe) {
        throw mcp::mcp_exception(mcp::error_code::internal_error, "Failed to execute command");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    // Return format matches agent_example.cpp's tool response format
    return {
        {
            {"type", "text"},
            {"text", result}
        }
    };
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

// Message display - similar to agent_example.cpp but simplified
// We only handle text content and tool calls
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
    // Parse config - simpler than agent_example.cpp
    config = parse_config(argc, argv);
    if (config.model_path.empty()) {
        std::cerr << "Model path required (-m)\n";
        return 1;
    }

    // Initialize Llama model - not in agent_example.cpp
    // This replaces the HTTP client setup in agent_example.cpp
    ggml_backend_load_all();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config.n_gpu_layers;
    llama_model * model = llama_model_load_from_file(config.model_path.c_str(), model_params);
    if (!model) {
        std::cerr << "Failed to load model\n";
        return 1;
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = config.n_ctx;
    ctx_params.n_batch = config.n_batch;
    llama_context * ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        std::cerr << "Failed to create context\n";
        return 1;
    }

    // Server setup - similar to agent_example.cpp
    // Both create a server and register tools
    mcp::server server("localhost", config.port);
    server.set_server_info("LlamaShellServer", "0.1.0");
    mcp::json capabilities = {
        {"tools", mcp::json::object()}
    };
    server.set_capabilities(capabilities);

    // Tool registration - similar pattern to agent_example.cpp
    // but with shell_command instead of calculator
    mcp::tool shell_tool = mcp::tool_builder("shell_command")
        .with_description("Execute a shell command and return the output")
        .with_string_param("command", "The shell command to execute")
        .build();

    server.register_tool(shell_tool, shell_command_handler);

    // Start server (non-blocking) - identical to agent_example.cpp
    server.start(false);

    // Client setup - identical to agent_example.cpp
    mcp::sse_client client("localhost", config.port);
    client.set_timeout(10);

    bool initialized = client.initialize("LlamaShellClient", "0.1.0");
    if (!initialized) {
        std::cerr << "Failed to initialize connection to server\n";
        return 1;
    }

    // Tool discovery - similar to agent_example.cpp
    // Both get tools from server and prepare them for LLM
    std::vector<common_chat_tool> llm_tools;
    {
        auto tools = client.get_tools();
        std::cout << "\nAvailable tools:\n";
        for (const auto& tool : tools) {
            std::cout << "- " << tool.name << ": " << tool.description << "\n";
            llm_tools.push_back({
                tool.name,
                tool.description,
                tool.parameters_schema.dump()
            });
        }
        std::cout << std::endl;
    }

    // Chat initialization - similar structure but local model vs API
    common_chat_templates_ptr chat_templates = common_chat_templates_init(model, "");
    std::vector<common_chat_msg> messages = {
        {
            "system",
            config.system_prompt,
            {}, {}, "", "", ""
        }
    };

    // Main chat loop - similar flow to agent_example.cpp but with local inference
    while (true) {
        std::cout << "\n>>> ";
        std::string prompt;
        readline_utf8(prompt, false);

        messages.push_back({
            "user",
            prompt,
            {}, {}, "", "", ""
        });

        // Chat template setup - similar to agent_example.cpp
        common_chat_templates_inputs inputs;
        inputs.messages = messages;
        inputs.tools = llm_tools;
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_AUTO;
        inputs.add_generation_prompt = true;
        inputs.use_jinja = true;

        // Template application - similar in both
        auto chat_params = common_chat_templates_apply(chat_templates.get(), inputs);

        // Local model inference - replaces ask_tool() in agent_example.cpp
        const int n_prompt = -llama_tokenize(vocab, chat_params.prompt.c_str(), chat_params.prompt.size(), NULL, 0, true, true);
        std::vector<llama_token> prompt_tokens(n_prompt);
        if (llama_tokenize(vocab, chat_params.prompt.c_str(), chat_params.prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true) < 0) {
            std::cerr << "Failed to tokenize prompt\n";
            return 1;
        }

        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
        auto sparams = llama_sampler_chain_default_params();
        sparams.no_perf = false;
        llama_sampler * smpl = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

        std::string response_text;
        for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + 256; ) {
            if (llama_decode(ctx, batch)) {
                std::cerr << "Failed to eval\n";
                return 1;
            }
            n_pos += batch.n_tokens;
            llama_token new_token_id = llama_sampler_sample(smpl, ctx, -1);
            if (llama_vocab_is_eog(vocab, new_token_id)) {
                break;
            }
            char buf[128];
            int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                std::cerr << "Failed to convert token\n";
                return 1;
            }
            std::string s(buf, n);
            response_text += s;
            printf("%s", s.c_str());
            fflush(stdout);
            batch = llama_batch_get_one(&new_token_id, 1);
        }
        printf("\n\n");

        // Response parsing - similar to agent_example.cpp
        common_chat_syntax syntax;
        syntax.format = chat_params.format;
        syntax.parse_tool_calls = true;
        common_chat_msg parsed_response = common_chat_parse(response_text, false, syntax);

        messages.push_back(parsed_response);

        // Tool execution - similar flow to agent_example.cpp
        if (!parsed_response.tool_calls.empty()) {
            for (const auto& tool_call : parsed_response.tool_calls) {
                try {
                    std::string tool_name = tool_call.name;
                    std::cout << "\nCalling tool " << tool_name << "...\n\n";

                    mcp::json args = mcp::json::parse(tool_call.arguments);
                    if (config.confirm_commands) {
                        std::cout << "Execute this command? (y/N): ";
                        std::string response;
                        std::getline(std::cin, response);
                        if (response != "y" && response != "Y") {
                            std::cout << "Command execution cancelled.\n";
                            continue;
                        }
                    }

                    // Tool execution through client - identical to agent_example.cpp
                    mcp::json result = client.call_tool(tool_name, args);
                    auto content = result.value("content", mcp::json::array());
                    std::cout << "\nResult:\n" << content << "\n\n";

                    messages.push_back({
                        "tool",
                        content.dump(),
                        {}, {}, "", "", tool_call.id
                    });
                } catch (const std::exception& e) {
                    messages.push_back({
                        "tool",
                        std::string("Error: ") + e.what(),
                        {}, {}, "", "", tool_call.id
                    });
                }
            }
        }

        llama_sampler_free(smpl);
    }

    // Cleanup - additional model cleanup not in agent_example.cpp
    llama_free(ctx);
    llama_model_free(model);
    return 0;
} 