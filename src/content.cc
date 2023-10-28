#include "content.h"

// deps
#include <spdlog/spdlog.h>

namespace content {
namespace {

Config gConfig;
Page gStartPage;

template <typename T>
using UnMap = std::unordered_map<std::string, T>;

UnMap<TgBot::InputFile::Ptr> gFiles;
UnMap<TgBot::InlineKeyboardButton::Ptr> gButtons;
UnMap<TgBot::InlineKeyboardMarkup::Ptr> gMarkups;

UnMap<Page> gPages;
UnMap<Dialog> gDialogs;

}  // namespace

// inner node decode, true -> exception
bool MissField(const YAML::Node& node,
               std::initializer_list<const char*> list) {
  for (const auto name : list) {
    if (!node[name]) {
      spdlog::error("{}: {}", __FUNCTION__, name);
      return true;
    }
  }
  return false;
}

// inner node decode, base and custom types
template <typename T>
T Optional(const YAML::Node& node, T&& opt) {
  return (node) ? node.as<T>() : std::forward<T>(opt);
}

// inner node decode, value from map
template <typename T>
T OptionalMap(const YAML::Node& node, const UnMap<T>& map) {
  if (!node) return T{};

  std::string as_str = node.as<std::string>();
  auto it = map.find(as_str);
  if (it != map.end()) {
    return it->second;
  } else {
    spdlog::warn("{}: Value was not found: [{}]", __FUNCTION__, as_str);
    return T{};
  }
}

// std::variant as ref, can't return multiple types
void TextOrFile(const YAML::Node& node, content::Text& text) {
  if (!node) {
    text = "";
    return;
  }

  std::string as_str = node.as<std::string>();
  auto it = content::gFiles.find(as_str);
  if (it != content::gFiles.end()) {
    text = it->second;
  } else {
    text = as_str;
  }
}

// safe read, for complex types and loops (outer try-catch lose data)
// try-catch: print incorrect type and field
template <typename T>
bool ReadNode(const YAML::Node& node, T& obj, const std::string& node_path) {
  if (!node) {
    spdlog::error("{}: '{}' not found!", __FUNCTION__, node_path);
    return false;
  }

  try {
    obj = std::move(node.as<T>());
    return true;
  } catch (const std::exception& e) {
    YAML::Emitter emit;
    emit << node;
    spdlog::error("{}: '{}', {}\n{}", __FUNCTION__, node_path, e.what(),
                  emit.c_str());
    return false;
  }
}

}  // namespace content

namespace YAML {
// all functions should be called via ReadNode with try-catch
// decode 'false' prints line and column
using namespace content;

template <>
struct convert<Config::SpamFilter> {
  static bool decode(const Node& node, Config::SpamFilter& filter) {
    if (MissField(node, {"every_n_sec", "msg_count", "block_time_sec"}))
      return false;

    filter.every_n_sec = node["every_n_sec"].as<int32_t>();
    filter.msg_count = node["msg_count"].as<int32_t>();
    filter.block_time_sec = node["block_time_sec"].as<int32_t>();
    return true;
  }
};

template <>
struct convert<Config::DialogCleaner> {
  static bool decode(const Node& node, Config::DialogCleaner& cleaner) {
    if (MissField(node, {"every_n_sec", "lifetime_sec"})) return false;

    cleaner.every_n_sec = node["every_n_sec"].as<int32_t>();
    cleaner.lifetime_sec = node["lifetime_sec"].as<int32_t>();
    return true;
  }
};

template <>
struct convert<Config::WriteStats> {
  static bool decode(const Node& node, Config::WriteStats& stats) {
    if (MissField(node, {"every_n_sec"})) return false;

    stats.every_n_sec = node["every_n_sec"].as<int32_t>();
    return true;
  }
};

template <>
struct convert<Config> {
  static bool decode(const Node& node, Config& cfg) {
    if (MissField(node, {"token"})) return false;

    cfg.token = node["token"].as<std::string>();
    cfg.name = Optional(node["name"], std::string{});
    cfg.username = Optional(node["username"], std::string{});
    cfg.spam_filter = Optional(node["spam_filter"], Config::SpamFilter{});
    cfg.dialog_cleaner =
        Optional(node["dialog_cleaner"], Config::DialogCleaner{});
    cfg.write_stats = Optional(node["write_stats"], Config::WriteStats{});

    return true;
  }
};

template <>
struct convert<TgBot::InputFile::Ptr> {
  static bool decode(const Node& node, TgBot::InputFile::Ptr& file) {
    if (MissField(node, {"path", "mime"})) return false;

    file = TgBot::InputFile::fromFile(node["path"].as<std::string>(),
                                      node["mime"].as<std::string>());

    // path or mime is incorrect -> nullptr
    if (file == nullptr) return false;

    return true;
  }
};

template <>
struct convert<TgBot::InlineKeyboardButton::Ptr> {
  static bool decode(const Node& node, TgBot::InlineKeyboardButton::Ptr& btn) {
    if (MissField(node, {"text"})) return false;

    // don't use default ctor() -> nullptr
    btn = std::make_shared<TgBot::InlineKeyboardButton>();
    btn->text = node["text"].as<std::string>();
    btn->callbackData = Optional(node["callback_data"], std::string{});
    btn->url = Optional(node["url"], std::string{});
    return true;
  }
};

// layout of buttons
using Markup1D = std::vector<std::string>;
void CreateMarkup1D(const Node& node,
                    TgBot::InlineKeyboardMarkup::Ptr& tg_markup) {
  const Markup1D row = node.as<Markup1D>();
  tg_markup = std::make_shared<TgBot::InlineKeyboardMarkup>();
  auto& tg_row = tg_markup->inlineKeyboard.emplace_back();
  tg_row.reserve(row.size());
  for (const auto& btn_name : row) {
    auto it = gButtons.find(btn_name);
    if (it != gButtons.end()) {
      tg_row.push_back(it->second);
    } else {
      spdlog::warn("{}: Button was not found: [{}]", __FUNCTION__, btn_name);
    }
  }
}

using Markup2D = std::vector<Markup1D>;
void CreateMarkup2D(const Node& node,
                    TgBot::InlineKeyboardMarkup::Ptr& tg_markup) {
  const Markup2D markup = node.as<Markup2D>();
  tg_markup = std::make_shared<TgBot::InlineKeyboardMarkup>();
  tg_markup->inlineKeyboard.reserve(markup.size());
  for (const auto& row : markup) {
    auto& tg_row = tg_markup->inlineKeyboard.emplace_back();
    tg_row.reserve(row.size());
    for (const auto& btn_name : row) {
      auto it = gButtons.find(btn_name);
      if (it != gButtons.end()) {
        tg_row.push_back(it->second);
      } else {
        spdlog::warn("{}: Button was not found: [{}]", __FUNCTION__, btn_name);
      }
    }
  }
}

template <>
struct convert<TgBot::InlineKeyboardMarkup::Ptr> {
  static bool decode(const Node& node,
                     TgBot::InlineKeyboardMarkup::Ptr& tg_markup) {
    if (!node.IsSequence()) return false;

    if ((*node.begin()).size()) {
      CreateMarkup2D(node, tg_markup);
    } else {
      CreateMarkup1D(node, tg_markup);
    }

    return true;
  }
};

template <>
struct convert<Post> {
  static bool decode(const Node& node, Post& post) {
    if (MissField(node, {"text"})) return false;

    TextOrFile(node["text"], post.text);
    post.media = OptionalMap(node["media"], gFiles);
    post.markup = OptionalMap(node["markup"], gMarkups);
    return true;
  }
};

template <>
struct convert<Dialog::Entry> {
  static bool decode(const Node& node, Dialog::Entry& entry) {
    if (MissField(node, {"post"})) return false;

    entry.post = node["post"].as<Post>();
    entry.label = Optional(node["label"], std::string{});
    entry.await = Optional(node["await"], std::string{});
    return true;
  }
};

template <>
struct convert<Dialog> {
  static bool decode(const Node& node, Dialog& dialog) {
    if (MissField(node, {"name", "type", "entries"})) return false;

    dialog.name = node["name"].as<std::string>();
    dialog.type = node["type"].as<std::string>();
    dialog.entries = node["entries"].as<std::vector<Dialog::Entry>>();
    return true;
  }
};

}  // namespace YAML

namespace content {

bool LoadYaml(YAML::Node& node, const char* filename) {
  try {
    spdlog::info("Loading YAML from '{}'...", filename);
    node = YAML::LoadFile(filename);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("{}: {}", __FUNCTION__, e.what());
    return false;
  }
}

template <typename T>
void LoadUnMap(const YAML::Node& node, UnMap<T>& map,
               const std::string& node_path) {
  if (node) {
    map.reserve(node.size());
    for (const auto& item : node) {
      const auto& key = item.first.as<std::string>();
      const auto& value = item.second;
      T obj;
      // inner try-catch or lose data
      if (ReadNode<T>(value, obj, fmt::format("{}/{}", node_path, key))) {
        map.try_emplace(key, std::move(obj));
      }
    }
  } else {
    spdlog::error("{}: '{}' not found!", __FUNCTION__, node_path);
  }
}

bool Initialize() {
  YAML::Node config;
  YAML::Node content;

  if (!LoadYaml(config, kConfigFilename)) return false;
  if (!LoadYaml(content, kContentFilename)) return false;

  if (!ReadNode(config, gConfig, "config")) {
    return false;
  }

  LoadUnMap(content["files"], gFiles, "content/files");
  LoadUnMap(content["buttons"], gButtons, "content/buttons");
  LoadUnMap(content["markups"], gMarkups, "content/markups");
  if (!ReadNode(content["start_page"], gStartPage, "content/start_page")) {
    gStartPage = Page{{Post{"Hi", nullptr, nullptr}}};
  }
  LoadUnMap(content["pages"], gPages, "content/pages");
  LoadUnMap(content["dialogs"], gDialogs, "content/dialogs");

  return true;
}

const Config& GetConfig() { return gConfig; }

const Page& GetStartPage() { return gStartPage; }

void GatherPages(std::vector<Callback<Page>>& cb) {
  cb.reserve(gPages.size());
  for (auto& [key, val] : gPages) {
    cb.emplace_back(key, val);
  }
}

void GatherDialogs(std::vector<Callback<Dialog>>& cb) {
  cb.reserve(gDialogs.size());
  for (auto& [key, val] : gDialogs) {
    cb.emplace_back(key, val);
  }
}

TgBot::InputFile::Ptr GetFile(const std::string& key) { return gFiles.at(key); }

TgBot::InlineKeyboardButton::Ptr GetButton(const std::string& key) {
  return gButtons.at(key);
}

TgBot::InlineKeyboardMarkup::Ptr GetMarkup(const std::string& key) {
  return gMarkups.at(key);
};

const Page& GetPage(const std::string& key) { return gPages.at(key); }

const Dialog& GetDialog(const std::string& key) { return gDialogs.at(key); }

}  // namespace content