#pragma once

#include "classes.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>

class NewsLogic {
private:
    Config config_;
    mutable std::shared_mutex mutex_;
    std::string db_file_ = "times_database.json";

    std::unordered_map<std::string, Reporter> reporters_;
    std::unordered_map<std::string, ReporterStats> stats_;
    std::unordered_map<std::string, Story> stories_;
    std::unordered_map<std::string, std::string> processed_notes_;

public:
    explicit NewsLogic(Config config) : config_(config) {}

    void add_client(const Reporter& client) {
        std::unique_lock lock(mutex_);
        reporters_[client.id] = client;
        if (stats_.find(client.id) == stats_.end()) {
            stats_[client.id] = ReporterStats{};
        }
        save_db();
    }

    void update_trust(const std::string& rep_id, double new_trust) {
        std::unique_lock lock(mutex_);
        if (reporters_.find(rep_id) != reporters_.end()) {
            reporters_[rep_id].trust = new_trust;
            save_db();
        }
    }

    bool authenticate(const std::string& rep_id, std::string& out_name) {
        std::shared_lock lock(mutex_);
        auto it = reporters_.find(rep_id);
        if (it != reporters_.end()) {
            out_name = it->second.name;
            return true;
        }
        return false;
    }

    SubmitStatus submit_note(const Note& note) {
        std::unique_lock lock(mutex_);
        SubmitStatus result;

        auto rep_it = reporters_.find(note.rep_id);
        if (rep_it == reporters_.end()) {
            result.status = NoteStatus::Rejected;
            result.reason = "Автор неизвестен";
            return result;
        }
        const auto& rep = rep_it->second;

        if (processed_notes_.find(note.id) != processed_notes_.end()) {
            result.status = NoteStatus::Duplicate;
            result.reason = "Note already processed";
            return result;
        }

        std::string story_key = get_story_key(note);
        auto& story = stories_[story_key];
        if (story.id.empty()) {
            fill_story(story, note);
        }

        if (note.timestamp < story.last_update) {
            return process_past_note(note, story, story_key);
        }

        if (story.printed) {
            return process_printed(note);
        }

        processed_notes_[note.id] = note.rep_id;
        stats_[note.rep_id].total_accepted++;
        stats_[note.rep_id].stories.insert(story_key);

        story.trust = lower_trust(
            story.trust, 
            story.last_update, 
            note.timestamp, 
            config_.half_life
        );
        story.last_update = note.timestamp;

        bool is_new = (story.authors_unique.find(note.rep_id) == story.authors_unique.end());
        story.notes.push_back(note);
        if (is_new) {
            story.authors_unique.insert(note.rep_id);
            story.authors.push_back(note.rep_id);
            story.trust += rep.trust;
            if (story.authors.size() == 1) {
                stats_[note.rep_id].scoops++;
            }
        }

        result.status = NoteStatus::Accepted;
        if (story.trust >= config_.print_trust && !story.printed) {
            print(story, result);
        }
        save_db();
        return result;
    }

    bool get_client_stats(const std::string& rep_id, ReporterStats& out_stats) const {
        std::shared_lock lock(mutex_);
        auto it = stats_.find(rep_id);
        if (it != stats_.end()) {
            out_stats = it->second;
            return true;
        }
        return false;
    }

    std::unordered_map<std::string, ReporterStats> get_all_stats() const {
        std::shared_lock lock(mutex_);
        return stats_;
    }

    std::unordered_map<std::string, Reporter> get_all_clients() const {
        std::shared_lock lock(mutex_);
        return reporters_;
    }

    void save_db() const {
        try {
            nlohmann::json j;
            j["clients"] = reporters_;
            j["stats"] = stats_;
            j["stories"] = stories_;
            j["processed_notes"] = processed_notes_;

            std::ofstream file(db_file_);
            if (file.is_open()) {
                file << j.dump(4);
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    bool load_from_disk(const std::string& path = "times_database.json") {
        std::unique_lock lock(mutex_);
        db_file_ = path;
        
        std::ifstream file(db_file_);
        if (!file.is_open()) {
            std::cout << "БД не найдена.\n";
            return false;
        }

        try {
            nlohmann::json j;
            file >> j;

            if (j.contains("clients")) reporters_ = j["clients"].get<std::unordered_map<std::string, Reporter>>();
            if (j.contains("stats")) stats_ = j["stats"].get<std::unordered_map<std::string, ReporterStats>>();
            if (j.contains("stories")) stories_ = j["stories"].get<std::unordered_map<std::string, Story>>();
            if (j.contains("processed_notes")) processed_notes_ = j["processed_notes"].get<std::unordered_map<std::string, std::string>>();

            std::cout << "БД загружена\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

private:
    std::string get_story_key(const Note& note) {
        return note.category + ":" + note.topic;
    }

    void fill_story(Story& story, const Note& note) {
        story.id = get_story_key(note);
        story.category = note.category;
        story.topic = note.topic;
        story.last_update = note.timestamp;
    }

    SubmitStatus process_past_note(const Note& note, const Story& story, const std::string& key) {
        SubmitStatus result;
        processed_notes_[note.id] = note.rep_id;
        stats_[note.rep_id].total_accepted++;
        stats_[note.rep_id].stories.insert(key);
        
        result.status = NoteStatus::Accepted;
        result.reason = "Принята заметка из прошлого";
        save_db();
        return result;
    }

    SubmitStatus process_printed(const Note& note) {
        SubmitStatus result;
        processed_notes_[note.id] = note.rep_id;
        result.status = NoteStatus::Rejected;
        result.reason = "Заметка уже напечатана";
        return result;
    }

    void print(Story& story, SubmitStatus& result) {
        story.printed = true;
        result.printed = true;

        for (size_t i = 0; i < story.authors.size(); ++i) {
            const std::string& cid = story.authors[i];
            stats_[cid].total_printed++;
            stats_[cid].rank_sum += i + 1;

            if (i < config_.congrats_cnt) {
                result.notifications.push_back({cid, "Поздравляем, ваша заметка попала в печать. Вы были " + 
                    std::to_string(i + 1) + "-м, кто сказал нам об этой новости."});
            }
        }
    }
};
