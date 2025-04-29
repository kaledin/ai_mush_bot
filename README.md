This is a simple bot that uses local Ollama to talk to your MUSH/MUX/MOO game.

Prerequisites:
1. Local Ollama with a working model (this has been tested on a small llama3.2 3b quantized model, works fast, only needs 8Gb of RAM)
2. Character account on a MUSH/MUX/MOO server

Howto install:
0. Make sure you have GCC and Curl (those are usually bundled with basic-devel type of packages)
1. Edit the ai_mush_bot.cpp file, fill out:
 * HOST - game server address
 * PORT - game server port
 * BOT_NAME - bot character name
 * BOT_PASSWORD - bot password
 * MODEL - Ollama model name used
 * PROMPT - Pre-prompt the bot as you see fit

 2. Compile with:
 g++ -std=c++23 -o ai_mush_bot ai_mush_bot.cpp -lcurl

 3. Run
 ./ai_mush_bot
