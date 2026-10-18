#include "server.hpp"
#include "editor.hpp"

int main() {
    try {
        asio::io_context io_context;
        NewsLogic engine(Config{1.0, 6.0, 3});

        if (!engine.load_from_disk("times_database.json")) {
            std::cout << "Первичная инициализация корреспондентов\n";
            engine.add_client({"sacharissa", "Сахарисса Крипслок", Role::Staff, 1.00});
            engine.add_client({"tonny", "Избирательно Честный Тонни", Role::Freelancer, 0.25});
            engine.add_client({"pew", "Иногда Слепой Пью", Role::Freelancer, 0.35});
        }

        Editor console(engine, io_context);
        console.start();
        std::cout << "Сервер запущен\n";
        Server server(io_context, 8080, engine);

        io_context.run();
        console.join();
        std::cout << "Сервер остановлен\n";

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
