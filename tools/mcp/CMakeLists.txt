option(LLAMA_BUILD_MCP_CLIENT "Build the mcp-cli tool" OFF)
option(LLAMA_BUILD_SIMPLE_MCP "Build the simple-mcp tool" OFF)

# If any tool is enabled, automatically enable cpp-mcp
if(LLAMA_BUILD_MCP_CLI OR LLAMA_BUILD_SIMPLE_MCP OR LLAMA_BUILD_SIMPLE_MCP_IMPROVED)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/cpp-mcp")
        add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/cpp-mcp vendor_cpp_mcp_build)
        # Make the mcp library available to subdirectories
        set(MCP_LIBRARY mcp)
    endif()
endif()

if(LLAMA_BUILD_MCP_CLIENT)
    add_subdirectory(simple-mcp-client)
endif()
if(LLAMA_BUILD_SIMPLE_MCP)
    add_subdirectory(simple-mcp-agent)
endif()
