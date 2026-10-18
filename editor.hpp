#pragma once
#include "engine.hpp"

#include <asio.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>

static constexpr std::size_t id_len = 20, name_len = 50;

class Editor {
public:
    Editor(NewsLogic& engine, asio::io_context& io_context)
        : engine_(engine), io_context_(io_context) {}

    void start() {
        thread_ = std::thread(&Editor::run, this);
    }

    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void print_help() {
        std::cout << "help - помощь\n"
                  << "stats - статистика\n"
                  << "add <id> <role> <trust> <name> - добавить корреспондента\n"
                  << "trust <rep_id> <score> - изменить доверие корреспондента\n"
                  << "exit - остановить сервер\n\n";
    }

    void print_stats() {
        auto stats = engine_.get_all_stats();
        auto reporters = engine_.get_all_clients();
        std::cout << std::left 
                  << std::setw(12) << "ID " 
                  << std::setw(25) << "Имя " 
                  << std::setw(10) << "Доверие " 
                  << std::setw(8)  << "Принято " 
                  << std::setw(8)  << "В печати " 
                  << std::setw(8)  << "Сенсации " << "\n";

        for (const auto& [id, profile] : reporters) {
            auto stat_it = stats.find(id);
            int accepted = 0, printed = 0, scoops = 0;

            if (stat_it != stats.end()) {
                const auto& s = stat_it->second;
                accepted = s.total_accepted;
                printed = s.total_printed;
                scoops = s.scoops;
            }

            std::cout << std::left 
                      << std::setw(12) << id 
                      << std::setw(25) << profile.name << " "
                      << std::setw(10) << std::fixed << std::setprecision(2) << profile.trust
                      << std::setw(8)  << accepted 
                      << std::setw(8)  << printed 
                      << std::setw(8)  << scoops << "\n";
        }
    }

    void command_trust(std::stringstream& ss) {
        std::string rep_id;
        double new_trust;
        if (!(ss >> rep_id >> new_trust)) {
            std::cout << "Формат: trust <rep_id> <score>\n";
            return;
        }
        
        engine_.update_trust(rep_id, new_trust);
        std::cout << "Успешно\n";
    }

    void command_add(std::stringstream& ss) {
        std::string id, role_str;
        double trust;
        
        if (!(ss >> id >> role_str >> trust)) {
            std::cout << "Формат: add <id> <role> <trust> <Имя Фамилия...>\n"
                      << "Роли: staff, freelancer\n";
            return;
        }

        Role role;
        if (role_str == "staff") {
            role = Role::Staff;
        } else if (role_str == "freelancer") {
            role = Role::Freelancer;
        } else {
            std::cout << "Неизвестная роль\n";
            return;
        }

        std::string name;
        std::getline(ss, name);
        if (!name.empty() && name[0] == ' ') {
            name = name.substr(1);
        }
        if (name.empty()) {
            std::cout << "Имя не может быть пустым\n";
            return;
        }

        Reporter new_profile{id, name, role, trust};
        engine_.add_client(new_profile);
        std::cout << "Успешно\n";
    }


    void run() {
        print_help();
        std::string line;
        
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::string command;
            ss >> command;

            if (command == "help") {
                print_help();
            } else if (command == "status" || command == "stats") {
                print_stats();
            } else if (command == "add") {
                command_add(ss);
            } else if (command == "trust") {
                command_trust(ss);
            } else if (command == "exit") {
                io_context_.stop();
                break;
            } else {
                std::cout << "Неизвестная команда (справка - help):" << command << "\n";
            }
        }
    }

private:
    NewsLogic& engine_;
    asio::io_context& io_context_;
    std::thread thread_;
};
