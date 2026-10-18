#pragma once

#include "engine.hpp"

#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <mutex>
#include <queue>

using json = nlohmann::json;
using asio::ip::tcp;

class Session;

class Manager {
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::queue<std::string>> missed_notifs_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

public:
    void register_session(const std::string& rep_id, std::shared_ptr<Session> session);
    void unregister_session(const std::string& rep_id);
    void notify(const std::string& rep_id, const std::string& json);
};

class Session : public std::enable_shared_from_this<Session> {
private:
    tcp::socket socket_;
    asio::streambuf input_buf_;
    std::queue<std::string> write_queue_;
    std::mutex mutex_;
    
    NewsLogic& engine_;
    Manager& manager_;
    std::string rep_id_;

public:
    Session(tcp::socket socket, NewsLogic& engine, Manager& cm)
        : socket_(std::move(socket)), engine_(engine), manager_(cm) {}

    ~Session() {
        if (!rep_id_.empty()) {
            manager_.unregister_session(rep_id_);
        }
    }

    void start() {
        read_line();
    }

    void send(const std::string& msg) {
        auto self = shared_from_this();
        asio::post(socket_.get_executor(), [this, self, msg]() {
            std::lock_guard<std::mutex> lock(mutex_);
            write_queue_.push(msg + "\n");
            if (write_queue_.size() == 1) {
                do_write();
            }
        });
    }

private:
    void read_line() {
        auto self = shared_from_this();
        asio::async_read_until(socket_, input_buf_, '\n',
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::string line;
                    std::istream is(&input_buf_);
                    std::getline(is, line);
                    
                    process_msg(line);
                    read_line();
                }
            });
    }

    void process_handshake(const nlohmann::json_abi_v3_11_2::json& j) {
        std::string id = j.value("rep_id", ""), name;
        if (engine_.authenticate(id, name)) {
            rep_id_ = id;
            manager_.register_session(id, shared_from_this());
            send(json({{"type", "HANDSHAKE_ACK"}, {"status", "success"}, {"name", name}}).dump());
        } else {
            send(json({{"type", "HANDSHAKE_ACK"}, {"status", "error"}, {"reason", "Корреспондент не авторизован"}}).dump());
        }
    }

    void process_stats(const nlohmann::json_abi_v3_11_2::json& j) {
        ReporterStats stats;
        json reply;
        reply["type"] = "MY_STATS_REPLY";

        if (engine_.get_client_stats(rep_id_, stats)) {
            reply["total_accepted"] = stats.total_accepted;
            reply["total_printed"] = stats.total_printed;
            reply["scoops"] = stats.scoops;
            reply["avg_rank"] = (stats.total_printed > 0) ? (stats.rank_sum / stats.total_printed) : 0.0;
            reply["stories_count"] = stats.stories.size();
        } else {
            reply["total_accepted"] = 0;
            reply["total_printed"] = 0;
            reply["scoops"] = 0;
            reply["avg_rank"] = 0.0;
            reply["stories_count"] = 0;
        }
        
        send(reply.dump());
    }

    void process_submit(const nlohmann::json_abi_v3_11_2::json& j) {
        Note note;
        note.id = j.value("id", "");
        note.rep_id = rep_id_;
        note.category = j.value("category", "");
        note.topic = j.value("topic", "");
        note.content = j.value("content", "");
        note.timestamp = std::chrono::system_clock::now();

        auto res = engine_.submit_note(note);
        std::string status = (res.status == NoteStatus::Accepted) ? "ACCEPTED" :
                                 (res.status == NoteStatus::Duplicate) ? "DUPLICATE" :
                                 "REJECTED";
        json receipt = {{"type", "RECEIPT"}, {"id", note.id}, {"status", status}, {"reason", res.reason}};
        send(receipt.dump());

        for (const auto& [id, text] : res.notifications) {
            json notify = {{"type", "CONGRATULATION"}, {"text", text}};
            manager_.notify(id, notify.dump());
        }
    }

    void process_msg(const std::string& msg) {
        try {
            auto j = json::parse(msg);
            std::string type = j.value("type", "");

            if (type == "HANDSHAKE") {
                process_handshake(j);
                return;
            }

            if (rep_id_.empty()) {
                json response = {{"type", "ERROR"}, {"reason", "Рукопожатие не выполнено"}};
                send(response.dump());
                return;
            }

            if (type == "GET_MY_STATS") {
                process_stats(j);
                return;
            }

            if (type == "SUBMIT_NOTE") {
                process_submit(j);
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void do_write() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_queue_.front()),
            [this, self](std::error_code ec, std::size_t length) {
                if (!ec) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    write_queue_.pop();
                    if (!write_queue_.empty()) {
                        do_write();
                    }
                }
            });
    }
};

inline void Manager::register_session(const std::string& rep_id, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[rep_id] = session;
    auto& q = missed_notifs_[rep_id];
    while (!q.empty()) {
        session->send(q.front());
        q.pop();
    }
}

inline void Manager::unregister_session(const std::string& rep_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(rep_id);
}

inline void Manager::notify(const std::string& rep_id, const std::string& message_json) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(rep_id);
    if (it != sessions_.end()) {
        it->second->send(message_json);
    } else {
        missed_notifs_[rep_id].push(message_json);
    }
}

class Server {
private:
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    NewsLogic& engine_;
    Manager client_manager_;

public:
    Server(asio::io_context& io_context, short port, NewsLogic& engine)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), engine_(engine) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), engine_, client_manager_)->start();
            }
            do_accept();
        });
    }
};
