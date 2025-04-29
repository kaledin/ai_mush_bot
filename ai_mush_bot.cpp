#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <curl/curl.h>
#include <netdb.h>
#include <unistd.h>

// CONFIG
const std::string HOST = "<server address>";
const int PORT = 1701;
const std::string BOT_NAME = "<charname>";
const std::string BOT_PASSWORD = "<pass>"; // or "" if not needed
const std::string MODEL = "<ollama model name>";

const std::string PROMPT =
    "You are an NPC in a test-based RPG (MUSH style)"
    "You will stay in the main OOC Lobby of the game answering questions"
    "about the game. Answer with no special symbols or line breaks."
    "One short paragraph maximum";

// Curl write callback
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* response = static_cast<std::string*>(userp);
    size_t total_size = size * nmemb;
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Extract response from Ollama chunk
std::string extract_response_field(const std::string& chunk) {
    const std::string key = "\"content\":\"";
    size_t start = chunk.find(key);
    if (start == std::string::npos) return "";
    start += key.length();
    size_t end = chunk.find("\"", start);
    if (end == std::string::npos) return "";
    return chunk.substr(start, end - start);
}

bool is_done_chunk(const std::string& chunk) {
    return chunk.find("\"done\":true") != std::string::npos;
}

// Talk to Ollama
std::string query_ollama(const std::string& input) {
    CURL* curl = curl_easy_init();
    std::string raw_data;

    if (!curl) return "Failed to init curl";

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/chat");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw_data);

    std::string payload = R"({"model":")" + MODEL + R"(","messages":[)"
        R"({"role":"system","content":")" + PROMPT + R"("},)"
        R"({"role":"user","content":")" + input + R"("}]})";
    
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return "Curl error";
    }

    curl_easy_cleanup(curl);

    std::istringstream stream(raw_data);
    std::string line, result;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (is_done_chunk(line)) break;
        result += extract_response_field(line);
    }

    return result;
}

// Connect to MUX
int connect_to_mux() {
    struct hostent* server = gethostbyname(HOST.c_str());
    if (!server) {
        std::cerr << "Failed to resolve host\n";
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    std::memcpy(&serv_addr.sin_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Basic login
void login_to_mux(int sockfd) {
    std::string login = "connect " + BOT_NAME + " " + BOT_PASSWORD + "\n";
    send(sockfd, login.c_str(), login.length(), 0);
}

// Main loop
void run_bot() {
    int sock = connect_to_mux();
    if (sock < 0) return;

    login_to_mux(sock);

    char buffer[2048];
    std::string partial;

    while (true) {
        ssize_t n = read(sock, buffer, sizeof(buffer) - 1);
        if (n <= 0) break;

        buffer[n] = '\0';
        partial += buffer;

        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            partial.erase(0, pos + 1);

            std::cout << "[MUX] " << line << "\n";

            // Detect speech
            size_t say_pos = line.find(" says, \"");
            if (say_pos != std::string::npos) {
                size_t start = say_pos + 8;
                size_t end = line.find("\"", start);
                if (end != std::string::npos) {
                    std::string message = line.substr(start, end - start);
                    std::cout << "[USER] said: " << message << "\n";

                    std::string reply = query_ollama(message);
                    std::string say_cmd = "say " + reply + "\n";
                    send(sock, say_cmd.c_str(), say_cmd.length(), 0);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(sock);
}

int main() {
    run_bot();
    return 0;
}
