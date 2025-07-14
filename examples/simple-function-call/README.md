# Simple Function Call Example

A standalone executable that demonstrates function calling ***from scratch*** with llama.cpp, allowing natural language to shell command execution.

## What This Is

- Users input natural language requests
- The LLM generates appropriate shell commands using function calling
- Commands are executed and results returned
- The conversation continues with real command output

## How It Works

### Architecture
- **Standalone executable** - no server architecture needed
- **Direct function calling** within the same process
- **Real shell command execution** via `popen()`
- **JSON parsing** of LLM tool calls

### Tool Schema
The LLM has access to a single tool:
```json
{
  "name": "shell_command",
  "description": "Execute a shell command and return the output",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "The shell command to execute"
      }
    },
    "required": ["command"]
  }
}
```

### Basic Usage
```
./simple-function-call -m model.gguf -p "your request here"
```
### Command Line Arguments
- `--jinja` Is enabled by default
- `-m model.gguf` - Model file path (REQUIRED)
- `-p "prompt"` - User's request/command (REQUIRED)
- `--chat-template-file template.jinja` - Optional chat template override
- `--grammar "grammar"` - Optional grammar constraint
- `-ngl N` - Number of GPU layers
- `-n N` - Maximum number of tokens to generate
- `--confirm` - Ask for confirmation before executing commands

### Examples

```
./simple-function-call -m llama-3.2-1b.gguf -p "list all files in this directory" # I've had success even with this small model but I think it has trouble solving complex tasks it can still generate proper JSON for execution

{"name": "shell_command", "parameters": {"command": "ls -l"}}

Function calls detected:
  Function: shell_command
  Arguments: {"command":"ls -l"}
  Command: ls -l
  Result:
total 812
-rw-r--r--  1 user user  47860 Jul 13 00:09 AUTHORS
drwxr-xr-x 12 user user   4096 Jul 13 00:09 build
-rwxr-xr-x  1 user user  21760 Jul 13 00:09 build-xcframework.sh
drwxr-xr-x  2 user user   4096 Jul 13 00:09 ci
drwxr-xr-x  2 user user   4096 Jul 13 00:09 cmake
-rw-r--r--  1 user user   7973 Jul 13 00:09 CMakeLists.txt
-rw-r--r--  1 user user   4008 Jul 13 00:09 CMakePresets.json
-rw-r--r--  1 user user    434 Jul 13 00:09 CODEOWNERS
drwxr-xr-x  2 user user   4096 Jul 13 00:09 common
-rw-r--r--  1 user user   6510 Jul 13 00:09 CONTRIBUTING.md
-rwxr-xr-x  1 user user 317736 Jul 13 00:09 convert_hf_to_gguf.py
-rwxr-xr-x  1 user user  21163 Jul 13 00:09 convert_hf_to_gguf_update.py
-rwxr-xr-x  1 user user  19106 Jul 13 00:09 convert_llama_ggml_to_gguf.py
-rwxr-xr-x  1 user user  18624 Jul 13 00:09 convert_lora_to_gguf.py
drwxr-xr-x  5 user user   4096 Jul 13 00:09 docs
drwxr-xr-x 29 user user   4096 Jul 13 00:09 examples
-rw-r--r--  1 user user   1556 Jul 13 00:09 flake.lock
-rw-r--r--  1 user user   7465 Jul 13 00:09 flake.nix
drwxr-xr-x  5 user user   4096 Jul 13 00:09 ggml
drwxr-xr-x  5 user user   4096 Jul 13 00:09 gguf-py
drwxr-xr-x  2 user user   4096 Jul 13 00:09 grammars
drwxr-xr-x  2 user user   4096 Jul 13 00:09 include
-rw-r--r--  1 user user   1078 Jul 13 00:09 LICENSE
drwxr-xr-x  2 user user   4096 Jul 13 00:09 licenses
drwxr-xr-x  2 user user   4096 Jul 13 00:09 llamacppos
-rw-r--r--  1 user user  50442 Jul 13 00:09 Makefile
drwxr-xr-x  2 user user   4096 Jul 13 00:09 media
drwxr-xr-x  3 user user   4096 Jul 13 00:09 models
-rw-r--r--  1 user user    163 Jul 13 00:09 mypy.ini
drwxr-xr-x  3 user user   4096 Jul 13 00:09 pocs
-rw-r--r--  1 user user 124786 Jul 13 00:09 poetry.lock
drwxr-xr-x  2 user user   4096 Jul 13 00:09 prompts
-rw-r--r--  1 user user   1336 Jul 13 00:09 pyproject.toml
-rw-r--r--  1 user user    616 Jul 13 00:09 pyrightconfig.json
-rw-r--r--  1 user user  29793 Jul 13 00:09 README.md
drwxr-xr-x  2 user user   4096 Jul 13 00:09 requirements
-rw-r--r--  1 user user    551 Jul 13 00:09 requirements.txt
drwxr-xr-x  3 user user   4096 Jul 13 00:09 scripts
-rw-r--r--  1 user user   5347 Jul 13 00:09 SECURITY.md
drwxr-xr-x  2 user user   4096 Jul 13 00:09 src
drwxr-xr-x  2 user user   4096 Jul 13 00:09 tests
drwxr-xr-x 18 user user   4096 Jul 13 00:09 tools
drwxr-xr-x  8 user user   4096 Jul 13 00:09 vendor
-rw-r--r--  1 user user   1165 Jul 13 00:09 windows-compat-loop.cpp

Continuing conversation with command result...
/home/user/llamacomp/llama.cpp/src/llama-context.cpp:919: GGML_ASSERT(n_tokens_all <= cparams.n_batch) failed
/home/user/llamacomp/llama.cpp/build/bin/libggml-base.so(+0x12ff6) [0x7fdfb47bdff6]
/home/user/llamacomp/llama.cpp/build/bin/libggml-base.so(ggml_print_backtrace+0x204) [0x7fdfb47be434]
/home/user/llamacomp/llama.cpp/build/bin/libggml-base.so(ggml_abort+0x130) [0x7fdfb47be5d0]
/home/user/llamacomp/llama.cpp/build/bin/libllama.so(_ZN13llama_context6decodeERK11llama_batch+0x14c3) [0x7fdfb4a16223]
/home/user/llamacomp/llama.cpp/build/bin/libllama.so(llama_decode+0xe) [0x7fdfb4a1632e]
./build/bin/simple-function-call(+0x35ba6) [0x557f2b789ba6]
/usr/lib/libc.so.6(+0x276b5) [0x7fdfb41126b5]
/usr/lib/libc.so.6(__libc_start_main+0x89) [0x7fdfb4112769]
./build/bin/simple-function-call(+0x37615) [0x557f2b78b615]
Aborted (core dumped)
```

# Check system information

```
./simple-function-call -m llama-3.2-1b.gguf -p "check the current time" # might produce some JSON
{
    "type": "function",
    "function": {
        "name": "shell_command",
        "description": "Execute a shell command and return the output",
        "parameters": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "The shell command to execute"
                }
            },
            "required": [
                "command"
            ]
        }
    },
    "parameters": {
        "command": "date && echo Current date and time: $(date +'%Y-%m-%d %H:%M:%S')"
    }
}

Response:
{
    "type": "function",
    "function": {
        "name": "shell_command",
        "description": "Execute a shell command and return the output",
        "parameters": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "The shell command to execute"
                }
            },
            "required": [
                "command"
            ]
        }
    },
    "parameters": { # Pay attention here
        "command": "date && echo Current date and time: $(date +'%Y-%m-%d %H:%M:%S')"
    } # The command it tried to use was probably too complex for the actual tool
} # A better parser or an LLM would've done a better job

main: decoded 134 tokens in 5.85 s, speed: 22.89 t/s # A better parser bigger model should produce better results
```
### You'll have less of these types of problems with bigger models but you can't still nudge the smaller ones, if you have some knowledge you can always tell it to run commands directly but it defeats the purpose of it a bit, it would be good with some Speech to Text system where you wouldn't have to type but just say what you want the computer to do
```
./simple-function-call -m llama-3.2-1b-instruct.gguf -p "Check the current date" # And it will run the date command

{"type": "function", "name": "shell_command", "parameters": {"command": "date"}}

Function calls detected:
  Function: shell_command
  Arguments: {"command":"date"}
  Command: date
  Result:
Sun Jul 13 11:24:16 PM CEST 2025

Continuing conversation with command result...
<|python_tag|>{"type": "function", "name": "shell_command", "parameters": {"command": "date"}}
main: decoded 45 tokens in 6.26 s, speed: 7.19 t/s

# With confirmation ( For safety )
./simple-function-call -m llama-3.2-1b-instruct.gguf -p "delete all .tmp files" --confirm

# Using a specific chat template for better function calling
./simple-function-call -m qwen2.5-7b-instruct.gguf -p "list files in current directory" \
    --chat-template-file models/templates/Qwen-Qwen2.5-7B-Instruct.jinja

# Using a model with native function calling support
./simple-function-call -m llama-3.1-8b-instruct.gguf -p "check disk usage" \
    --chat-template-file models/templates/meta-llama-Llama-3.1-8B-Instruct.jinja
```
