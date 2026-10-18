#pragma once

#include <algorithm>
#include <cmath>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

enum class Role { Freelancer, Staff, Editor };
enum class NoteStatus { Accepted, Duplicate, Rejected };

struct Reporter {
    std::string id, name;
    Role role;
    double trust;
};

struct ReporterStats {
    int total_accepted{0}, total_printed{0}, scoops{0};
    double rank_sum{0};
    std::unordered_set<std::string> stories;
};

struct Note {
    std::string id, rep_id, category, topic, content;
    std::chrono::system_clock::time_point timestamp;
};

struct SubmitStatus {
    NoteStatus status;
    std::string reason;
    bool printed{false};
    std::vector<std::pair<std::string, std::string>> notifications;
};

struct Story {
    std::string id, category, topic;
    bool printed = false;
    double trust = 0.0;
    std::chrono::system_clock::time_point last_update;

    std::vector<std::string> authors;
    std::unordered_set<std::string> authors_unique;
    std::vector<Note> notes; 
};

struct Config {
    double print_trust{1.0}, half_life{6.0};
    int congrats_cnt{3};
};

inline double lower_trust(double initial_trust, std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point now, double half_life) {
    return initial_trust * std::pow(2.0,
        -static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count()) / (half_life * 3600));
}

inline std::string pad(const std::string& str, std::size_t len, char chr = ' ') {
    if (str.size() >= len) {
        return str;
    }
    return str + std::string(len - str.size(), chr);
}

NLOHMANN_JSON_SERIALIZE_ENUM(Role, {
    {Role::Freelancer, "Freelancer"},
    {Role::Staff, "Staff"},
    {Role::Editor, "Editor"},
})

inline void to_json(nlohmann::json& j, const Reporter& p) {
    j = nlohmann::json{{"id", p.id}, {"name", p.name}, {"role", p.role}, {"trust", p.trust}};
}
inline void from_json(const nlohmann::json& j, Reporter& p) {
    j.at("id").get_to(p.id);
    j.at("name").get_to(p.name);
    j.at("role").get_to(p.role);
    j.at("trust").get_to(p.trust);
}

inline void to_json(nlohmann::json& j, const ReporterStats& s) {
    j = nlohmann::json{
        {"total_accepted", s.total_accepted},
        {"total_printed", s.total_printed},
        {"scoops", s.scoops},
        {"rank_sum", s.rank_sum},
        {"stories", s.stories}
    };
}
inline void from_json(const nlohmann::json& j, ReporterStats& s) {
    j.at("total_accepted").get_to(s.total_accepted);
    j.at("total_printed").get_to(s.total_printed);
    j.at("scoops").get_to(s.scoops);
    j.at("rank_sum").get_to(s.rank_sum);
    j.at("stories").get_to(s.stories);
}

inline void to_json(nlohmann::json& j, const Note& n) {
    j = nlohmann::json{
        {"id", n.id},
        {"rep_id", n.rep_id},
        {"category", n.category},
        {"topic", n.topic},
        {"content", n.content},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(n.timestamp.time_since_epoch()).count()}
    };
}
inline void from_json(const nlohmann::json& j, Note& n) {
    j.at("id").get_to(n.id);
    j.at("rep_id").get_to(n.rep_id);
    j.at("category").get_to(n.category);
    j.at("topic").get_to(n.topic);
    j.at("content").get_to(n.content);
    
    int64_t duration;
    j.at("timestamp").get_to(duration);
    n.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(duration));
}

inline void to_json(nlohmann::json& j, const Story& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"category", s.category},
        {"topic", s.topic},
        {"printed", s.printed},
        {"trust", s.trust},
        {"last_update", std::chrono::duration_cast<std::chrono::seconds>(s.last_update.time_since_epoch()).count()},
        {"authors", s.authors},
        {"authors_unique", s.authors_unique},
        {"notes", s.notes}
    };
}
inline void from_json(const nlohmann::json& j, Story& s) {
    j.at("id").get_to(s.id);
    j.at("category").get_to(s.category);
    j.at("topic").get_to(s.topic);
    j.at("printed").get_to(s.printed);
    j.at("trust").get_to(s.trust);
    j.at("authors").get_to(s.authors);
    j.at("authors_unique").get_to(s.authors_unique);
    j.at("notes").get_to(s.notes);
    
    int64_t duration;
    j.at("last_update").get_to(duration);
    s.last_update = std::chrono::system_clock::time_point(std::chrono::seconds(duration));
}

