#pragma once

// deps
#include <tgbot/tgbot.h>
#include <yaml-cpp/yaml.h>
// global
#include <variant>

namespace content {

inline constexpr const char* kConfigFilename{"config.yaml"};
inline constexpr const char* kContentFilename{"content.yaml"};

struct Config {
  std::string token;
  std::string name;
  std::string username;
  struct SpamFilter {
    int32_t every_n_sec{60};
    int32_t msg_count{50};
    int32_t block_time_sec{60 * 60};
  } spam_filter;
  struct DialogCleaner {
    int32_t every_n_sec{60};
    int32_t lifetime_sec{60 * 60};
  } dialog_cleaner;
  struct WriteStats {
    int32_t every_n_sec{24 * 60 * 60};
  } write_stats;
};

using Text = std::variant<std::string, TgBot::InputFile::Ptr>;

inline const std::string& Extract(const Text& text) {
  if (std::holds_alternative<std::string>(text)) {
    return std::get<std::string>(text);
  } else {
    return std::get<TgBot::InputFile::Ptr>(text)->data;
  }
}

struct Post {
  Text text;
  TgBot::InputFile::Ptr media;
  TgBot::InlineKeyboardMarkup::Ptr markup;
};
using Page = std::vector<Post>;

struct Dialog {
  struct Entry {
    Post post;
    std::string label;
    std::string await;
  };
  std::string name;
  std::string type;
  std::vector<Entry> entries;
};

bool Initialize();

const Config& GetConfig();
const Page& GetStartPage();

// iterate over the vector for button callbacks (optimization)
// objects are loaded once at startup and static
template <typename T>
struct Callback {
  const std::string& key;
  const T& obj;
};
void GatherPages(std::vector<Callback<Page>>& cb);
void GatherDialogs(std::vector<Callback<Dialog>>& cb);

TgBot::InputFile::Ptr GetFile(const std::string& key);
TgBot::InlineKeyboardButton::Ptr GetButton(const std::string& key);
TgBot::InlineKeyboardMarkup::Ptr GetMarkup(const std::string& key);

const Page& GetPage(const std::string& key);
const Dialog& GetDialog(const std::string& key);

}  // namespace content