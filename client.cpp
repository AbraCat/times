#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <iomanip>
#include <mutex>
#include <string>
#include <thread>
#include <queue>
#include <random>

using json = nlohmann::json;
using asio::ip::tcp;

std::string generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 9);
    std::string res = "";
    for (int i = 0; i < 16; ++i) {
        res += std::string(1, '0' + dis(gen));
    }
    return res;
}

class Client {
public:
    Client(std::string host, std::string port, std::string rep_id)
        : server_host_(std::move(host)), server_port_(std::move(port)), rep_id_(std::move(rep_id)) {
        socket_ = std::make_unique<tcp::socket>(io_context_);
    }

    ~Client() {
        disconnect();
    }

    void start() {
        connect();
        thread_ = std::thread([this]() {
            asio::io_context::work work(io_context_);
            io_context_.run();
        });
    }

    void disconnect() {
        is_running_ = false;
        io_context_.stop();
        if (socket_ && socket_->is_open()) {
            std::error_code ec;
            socket_->close(ec);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void send_note(const std::string& category, const std::string& topic, const std::string& content) {
        if (!is_authenticated_) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::cout << "Рукопожатие не выполнено\n";
            return;
        }

        json j = {
            {"type", "SUBMIT_NOTE"},
            {"id", generate_id()},
            {"category", category},
            {"topic", topic},
            {"content", content}
        };
        write_line(j.dump());
    }

    void request_my_stats() {
        if (!is_authenticated_) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::cout << "Рукопожатие не выполнено.\n";
            return;
        }
        json j = { {"type", "GET_MY_STATS"} };
        write_line(j.dump());
    }

private:
    void connect() {
        try {
            tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(server_host_, server_port_);
            asio::connect(*socket_, endpoints);

            json handshake = {
                {"type", "HANDSHAKE"},
                {"rep_id", rep_id_}
            };
            std::string msg = handshake.dump() + "\n";
            asio::write(*socket_, asio::buffer(msg));

            read_line();

        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            std::cout << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (is_running_) connect();
        }
    }

    void read_line() {
        asio::async_read_until(*socket_, input_buf_, '\n',
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string line;
                    std::istream is(&input_buf_);
                    std::getline(is, line);
                    
                    process_server_message(line);
                    if (is_running_) {
                        read_line();
                    }
                } else {
                    handle_disconnect();
                }
            });
    }

    void write_line(const std::string& message) {
        auto msg_ptr = std::make_shared<std::string>(message + "\n");
        asio::async_write(*socket_, asio::buffer(*msg_ptr),
            [this, msg_ptr](std::error_code ec, std::size_t) {
                if (ec) {
                    handle_disconnect();
                }
            });
    }

    void process_server_message(const std::string& raw_msg) {
        try {
            auto j = json::parse(raw_msg);
            std::string type = j.value("type", "");

            std::lock_guard<std::mutex> lock(mutex_);

            if (type == "HANDSHAKE_ACK") {
                if (j.value("status", "") == "success") {
                    is_authenticated_ = true;
                    std::cout << "Авторизация успешна, " << j.value("name", "") << "\n";
                } else {
                    std::cout << "Автризация неуспешна: " << j.value("reason", "") << "\n";
                }
            } 
            else if (type == "RECEIPT") {
                std::cout << "Заметка " << j.value("id", "") << " Статус: " << j.value("status", "");
                if (j.contains("reason") && !j["reason"].get<std::string>().empty()) {
                    std::cout << " " << j["reason"].get<std::string>();
                }
                std::cout << "\n";
            } 
            else if (type == "CONGRATULATION") {
                std::cout << j.value("text", "") << "\n\n";
            }
            else if (type == "MY_STATS_REPLY") {
                std::cout << "Принято:  " << j.value("total_accepted", 0) << "\n"
                          << "Напечатано:  " << j.value("total_printed", 0) << "\n"
                          << "Сенсации: " << j.value("scoops", 0) << "\n"
                          << "Сюжетов: " << j.value("stories_count", 0) << "\n\n";
            }
        } catch (...) {}
    }

    void handle_disconnect() {
        is_authenticated_ = false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_running_) return;

        std::cout << "Соединение потеряно. Переподключение\n";
        socket_ = std::make_unique<tcp::socket>(io_context_);
        connect();
    }

private:
    asio::io_context io_context_;
    std::unique_ptr<tcp::socket> socket_;
    asio::streambuf input_buf_;
    std::thread thread_;
    std::mutex mutex_;
    
    bool is_authenticated_{false}, is_running_{true};
    std::string server_host_, server_port_, rep_id_;
};

void print_help() {
    std::cout << "Команды:\n"
              << "help - помощь\n"
              << "send - отправить заметку\n"
              << "stats - статистика\n"
              << "exit - выход\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Использование: client <host> <port> <id>\n";
        return 1;
    }

    std::string host = argv[1], port = argv[2], id = argv[3];
    Client client(host, port, id);
    client.start();

    std::string cmd;
    print_help();
    while (true) {    
        std::getline(std::cin, cmd);
        if (cmd == "exit") {
            break;
        }
        
        if (cmd == "help") {
            print_help();
        } else if (cmd == "send") {
            std::string category, topic, content;
            std::cout << "Рубрика: ";
            std::getline(std::cin, category);
            std::cout << "Тема: ";
            std::getline(std::cin, topic);
            std::cout << "Текст: ";
            std::getline(std::cin, content);

            client.send_note(category, topic, content);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } 
        else if (cmd == "stats") {
            client.request_my_stats();
        } else {
            std::cout << "Неизвестная команда: " << cmd << "\n";
        }
    }

    client.disconnect();
    return 0;
}
