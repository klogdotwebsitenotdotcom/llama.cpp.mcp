# First, ensure cpp-mcp is available
if(NOT TARGET mcp)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/cpp-mcp vendor_cpp_mcp_build)
endif()

# Create your executable
add_executable(simple-mcp-agent main.cpp)

# Link required libraries
target_link_libraries(simple-mcp-agent PRIVATE 
    mcp        # MCP library
    llama      # llama.cpp core library
    common     # llama.cpp common utilities
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add source files if needed
target_sources(simple-mcp-agent PRIVATE
    ${CMAKE_SOURCE_DIR}/common/arg.cpp          # For command line parsing
    ${CMAKE_SOURCE_DIR}/common/sampling.cpp     # For text sampling
    ${CMAKE_SOURCE_DIR}/common/json-schema-to-grammar.cpp  # For JSON handling
    ${CMAKE_SOURCE_DIR}/common/chat.cpp         # For chat templates
)

# Include all necessary directories
target_include_directories(simple-mcp-agent PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/cpp-httplib    # For httplib.h
    ${CMAKE_SOURCE_DIR}/vendor/nlohmann       # For nlohmann/json.hpp
    ${CMAKE_SOURCE_DIR}/vendor/cpp-mcp/include
    ${CMAKE_SOURCE_DIR}/vendor/cpp-mcp/common
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
)

target_compile_features(simple-mcp-improved PRIVATE cxx_std_17)

if (WIN32)
    target_link_libraries(simple-mcp-improved PRIVATE ws2_32)
endif() 