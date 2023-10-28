#include "bot.h"

// deps
#include <fmt/chrono.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
// global
#include <thread>
// local
#include "files.h"

std::string GetDateTime() {
  auto now = chrono::system_clock::now();
  auto unix_time = now.time_since_epoch();
  return fmt::format("datetime: {:%F} {:%R:%OS}\n", now, unix_time);
}

UnixTime GetUnixTime() {
  std::time_t now =
      chrono::system_clock::to_time_t(chrono::system_clock::now());
  return static_cast<UnixTime>(now);
}

void Bot::AppendCommandToMenu(const std::string& cmd, const std::string& desc) {
  auto& new_cmd =
      menu_cmds_.emplace_back(std::make_shared<TgBot::BotCommand>());
  new_cmd->command = cmd;
  new_cmd->description = desc;
}

bool Bot::IsUserBlocked(UserId user_id) const {
  std::scoped_lock lock(mut_spam_);
  return blacklist_.contains(user_id);
}

void Bot::SendPost(const MsgPtr& msg, const content::Post& post) const {
  if (post.media) {
    if (StringTools::startsWith(post.media->mimeType, "image")) {
      api_.sendPhoto(msg->chat->id, post.media, content::Extract(post.text), 0,
                     post.markup, kParseMode);
    }
    if (StringTools::startsWith(post.media->mimeType, "video")) {
      api_.sendVideo(msg->chat->id, post.media, true, 0, 0, 0, post.media,
                     content::Extract(post.text), 0, post.markup, kParseMode);
    }
  } else {
    api_.sendMessage(msg->chat->id, content::Extract(post.text), false, 0,
                     post.markup, kParseMode);
  }
}

void Bot::SendPage(const MsgPtr& msg, const content::Page& page) const {
  for (const auto& post : page) {
    SendPost(msg, post);
  }
}

std::string Bot::DownloadFileById(const std::string& file_id) const {
  auto file = api_.getFile(file_id);
  return api_.downloadFile(file->filePath);
}

void Bot::ReceiveUserAvatars(const MsgPtr& msg, size_t max_count) const {
  UserId user_id = msg->from->id;
  auto avatars = api_.getUserProfilePhotos(user_id);
  size_t counter = 0;
  for (const auto& row : avatars->photos) {
    if (counter == max_count) break;

    const auto& biggest = row.back()->fileId;
    fs::path path{fmt::format("./users/{}/avatars/{}.jpeg", user_id, biggest)};
    std::string data = DownloadFileById(biggest);
    files::WriteFile(path, data);

    ++counter;
  }
}

void Bot::StartDialog(const CallbackPtr& cb, const content::Dialog& dialog) {
  std::scoped_lock lock(mut_state_);
  // register a new dialog
  UserId user_id = cb->from->id;
  // each call to system clock cost ~100ns (optimization)
  auto now = chrono::system_clock::now();
  auto [it, success] = user_state_.try_emplace(user_id,  //
                                               dialog,   //
                                               0,        //
                                               now       //
  );
  // only one dialog per user
  if (success) {
    // record user information
    fs::path path{fmt::format("./users/{}/info.txt", user_id)};
    const auto& msg = cb->message;
    if (!fs::exists(path)) {
      std::string info;
      info += fmt::format("datetime: {:%F} {:%R:%OS}\n", now,
                          now.time_since_epoch());
      info += fmt::format("user_id: {}\n", user_id);
      info += fmt::format("username: {}\n", msg->chat->username);
      info += fmt::format("first_name: {}\n", msg->chat->firstName);
      info += fmt::format("last_name: {}\n\n", msg->chat->lastName);

      info += fmt::format("dialog: {}\n", dialog.name);
      files::WriteFile(path, info);
    } else {
      std::string data{fmt::format("\ndialog: {}\n", dialog.name)};
      files::AppendFile(path, data);
    }
    // send the first post
    SendPost(cb->message, dialog.entries[0].post);
    ++dialog_used_;
  }
}

void Bot::EndDialog(const CallbackPtr& cb) {
  std::scoped_lock lock(mut_state_);
  user_state_.erase(cb->from->id);
}

bool Bot::ReceiveMessages(const MsgPtr& msg, const std::string& label) const {
  const auto& text = msg->text;
  if (text.size() == 0) return false;

  UserId user_id = msg->from->id;
  fs::path path{fmt::format("./users/{}/info.txt", user_id)};
  std::string data{fmt::format("{}: {}\n", label, text)};
  files::AppendFile(path, data);
  return true;
}

bool Bot::ReceivePhotos(const MsgPtr& msg) const {
  const auto& photos = msg->photo;
  if (photos.size() == 0) return false;

  // download the last, the biggest photo
  const auto& file_id = photos.back()->fileId;
  UserId user_id = msg->from->id;
  fs::path path{fmt::format("./users/{}/{}.jpeg", user_id, file_id)};
  std::string data = DownloadFileById(file_id);
  files::WriteFile(path, data);
  return true;
}

bool Bot::ReceiveDocuments(const MsgPtr& msg) const {
  auto doc = msg->document;
  if (!doc) return false;

  UserId user_id = msg->from->id;
  fs::path path{fmt::format("./users/{}/{}", user_id, doc->fileName)};
  std::string data = DownloadFileById(doc->fileId);
  files::WriteFile(path, data);
  return true;
}

bool Bot::ProcessMessage(const MsgPtr& msg,
                         const content::Dialog::Entry& entry) const {
  // no await, the ending page of dialog
  const auto& await = entry.await;
  if (!await.size()) return true;

  if (StringTools::startsWith(await, "all")) {
    int32_t done = 0;
    done += ReceiveMessages(msg, entry.label);
    done += ReceivePhotos(msg);
    done += ReceiveDocuments(msg);
    return done;
  }
  if (StringTools::startsWith(await, "text")) {
    if (ReceiveMessages(msg, entry.label)) return true;
  }
  if (StringTools::startsWith(await, "files")) {
    int32_t done = 0;
    done += ReceivePhotos(msg);
    done += ReceiveDocuments(msg);
    return done;
  }
  return false;
}

void Bot::ProcessDialogs(const MsgPtr& msg) {
  std::scoped_lock lock(mut_state_);

  UserId user_id = msg->from->id;
  auto it = user_state_.find(user_id);
  if (it == user_state_.end()) return;
  auto& state = it->second;
  const auto& entries = state.dialog.entries;

  // process current user's message
  const auto& current = entries[state.step];
  bool msg_done = ProcessMessage(msg, current);
  // resend page to remind user what to do
  if (!msg_done) {
    SendPost(msg, current.post);
    return;
  }

  // send next post
  ++state.step;
  if (state.step < entries.size()) {
    const auto& next = entries[state.step];
    SendPost(msg, next.post);

    // the end, last page without await
    bool last_page = ((entries.size() - 1) == state.step);
    bool no_await = !next.await.size();
    if (last_page && no_await) {
      user_state_.erase(user_id);
    }
  } else {
    // the end, all pages with await
    user_state_.erase(user_id);
  }
}

Bot::Bot()
    : bot_(content::GetConfig().token),
      events_(bot_.getEvents()),
      api_(bot_.getApi()) {
  // before callbacks
  content::GatherPages(btn_pages_);
  content::GatherDialogs(btn_dialogs_);

  // all events in constructor
  events_.onCommand("start", [this](MsgPtr msg) {
    try {
      // skip bots and blocked users
      if (msg->from->isBot) return;
      if (IsUserBlocked(msg->from->id)) return;

      ReceiveUserAvatars(msg, 3);
      SendPage(msg, content::GetStartPage());
      ++visitors_count_;
    } catch (const std::exception& e) {
      spdlog::error("onCommand: /start, {}", e.what());
    }
  });

  // commands menu
  AppendCommandToMenu("start", "...");
  api_.setMyCommands(menu_cmds_);

  // user_id = cb->from->id
  // bot_id = cb->message->from->id
  events_.onCallbackQuery([this](CallbackPtr cb) {
    try {
      if (IsUserBlocked(cb->from->id)) return;

      if (StringTools::startsWith(cb->data, "start_page")) {
        SendPage(cb->message, content::GetStartPage());
      }

      for (const auto& item : btn_pages_) {
        if (StringTools::startsWith(cb->data, item.key)) {
          SendPage(cb->message, item.obj);
        }
      }

      for (const auto& item : btn_dialogs_) {
        if (StringTools::startsWith(cb->data, item.key)) {
          StartDialog(cb, item.obj);
        }
      }

      if (StringTools::endsWith(cb->data, "end_diag")) {
        EndDialog(cb);
      }

    } catch (const std::exception& e) {
      spdlog::error("onCallbackQuery: {}, {}", cb->data, e.what());
    }
  });

  // ignore unknown command
  events_.onUnknownCommand([this](MsgPtr msg) {});

  events_.onNonCommandMessage([this](MsgPtr msg) {
    try {
      // skip bots
      if (msg->from->isBot) return;

      {
        std::scoped_lock lock(mut_spam_);
        // skip blocked user
        if (blacklist_.contains(msg->from->id)) return;
        // count user messages to prevent spam
        ++user_msg_count_[msg->from->id];
      }

      ProcessDialogs(msg);
    } catch (const std::exception& e) {
      spdlog::error("onNonCommandMessage: {}", e.what());
    }
  });
}

void Bot::StartService(MemFn mem_fn, int32_t every_n_sec) {
  std::thread t([this, mem_fn, every_n_sec]() {
    while (true) {
      (this->*mem_fn)();
      std::this_thread::sleep_for(chrono::seconds(every_n_sec));
    }
  });
  t.detach();
}

void Bot::ServiceWriteStats() {
  fs::path path{"./users/stats.txt"};
  std::string stats;
  stats += GetDateTime();
  stats += fmt::format("visitors_count: {}\n", visitors_count_);
  stats += fmt::format("dialog_used: {}\n", dialog_used_);
  files::AppendFile(path, stats);
}

void Bot::ServiceSpamFilter() {
  std::scoped_lock lock(mut_spam_);

  const auto& filter = content::GetConfig().spam_filter;
  UnixTime now = GetUnixTime();

  // first process blocks
  int32_t block_until = now + filter.block_time_sec;
  for (const auto& [user_id, count] : user_msg_count_) {
    if (count >= filter.msg_count) {
      blacklist_.try_emplace(user_id, block_until);
      spdlog::warn("Block User '{}' for {}s", user_id, filter.block_time_sec);
    }
  }

  // second process unlocks
  std::erase_if(blacklist_, [this, now](const std::pair<UserId, UnixTime>& kv) {
    const auto user_id = kv.first;
    const auto block_until = kv.second;
    if (now > block_until) {
      // reset msg counter
      user_msg_count_.at(user_id) = 0;
      spdlog::warn("Unblock User '{}'", user_id);
      return true;
    }
    return false;
  });
}

void Bot::ServiceDialogCleaner() {
  std::scoped_lock lock(mut_state_);

  std::erase_if(user_state_, [this](const std::pair<UserId, DialogState>& kv) {
    const auto& cleaner = content::GetConfig().dialog_cleaner;
    const auto user = kv.first;
    const auto& state = kv.second;

    chrono::nanoseconds diff = chrono::system_clock::now() - state.created;
    int32_t lifetime = chrono::duration_cast<chrono::seconds>(diff).count();
    if (lifetime > cleaner.lifetime_sec) {
      spdlog::info("Erase dialog for User: {}", user);
      return true;
    }
    return false;
  });
}

void Bot::LongPoll() {
  try {
    // skip already read old messages, when restart
    api_.deleteWebhook();

    TgBot::TgLongPoll long_poll(bot_);

    auto me = api_.getMe();
    spdlog::info("Bot Info:");
    spdlog::info("  id: {}", me->id);
    spdlog::info("  username: {}", me->username);
    spdlog::info("  first_name: {}", me->firstName);
    spdlog::info("  can_join_groups: {}", me->canJoinGroups);
    spdlog::info("  can_read_all_group_messages: {}",
                 me->canReadAllGroupMessages);

    // start services
    const auto& cfg = content::GetConfig();
    StartService(&Bot::ServiceWriteStats, cfg.write_stats.every_n_sec);
    StartService(&Bot::ServiceSpamFilter, cfg.spam_filter.every_n_sec);
    StartService(&Bot::ServiceDialogCleaner, cfg.dialog_cleaner.every_n_sec);

    while (true) {
      // fire when receive messages and over some time
      long_poll.start();
    }
  } catch (TgBot::TgException& e) {
    spdlog::error("Error: {}", e.what());
  }
}