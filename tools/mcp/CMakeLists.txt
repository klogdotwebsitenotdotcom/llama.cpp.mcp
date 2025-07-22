option(LLAMA_BUILD_SIMPLE_MCP_AGENT "Build the llama-simple-mcp-agent tool" OFF)
option(LLAMA_BUILD_SIMPLE_MCP_CLIENT "Build the llama-simple-mcp-client tool" OFF)

# If any tool is enabled, automatically enable cpp-mcp
if(LLAMA_BUILD_SIMPLE_MCP_AGENT OR LLAMA_BUILD_SIMPLE_MCP_CLIENT)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/cpp-mcp")
        add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/cpp-mcp vendor_cpp_mcp_build)
        set(MCP_LIBRARY mcp)
    endif()
endif()

if(LLAMA_BUILD_SIMPLE_MCP_AGENT)
    add_subdirectory(simple-mcp-agent)
endif()
if(LLAMA_BUILD_SIMPLE_MCP_CLIENT)
    add_subdirectory(simple-mcp-client)
endif()
