#pragma once

// deps
#include <tgbot/tgbot.h>
// global
#include <chrono>
#include <mutex>
// local
#include "content.h"

namespace chrono = std::chrono;

struct DialogState {
  const content::Dialog& dialog;
  size_t step;
  chrono::time_point<chrono::system_clock> created;
};

using MsgPtr = TgBot::Message::Ptr;
using CallbackPtr = TgBot::CallbackQuery::Ptr;
// in private chats, UserId == ChatId (msg->chat->id)
// msg->from->id
using UserId = int64_t;
// UNIX time, seconds since Epoch, used by Telegram
using UnixTime = int32_t;

class Bot {
 public:
  Bot();

 public:
  void LongPoll();

 private:
  TgBot::Bot bot_;
  // deps as referencies
  TgBot::EventBroadcaster& events_;
  const TgBot::Api& api_;
  // list of commands for menu button
  std::vector<TgBot::BotCommand::Ptr> menu_cmds_;
  // button callbacks from content
  std::vector<content::Callback<content::Page>> btn_pages_;
  std::vector<content::Callback<content::Dialog>> btn_dialogs_;
  // UserId as key and unique folder name
  std::unordered_map<UserId, DialogState> user_state_;
  mutable std::mutex mut_state_;
  // simple spam filter
  std::unordered_map<UserId, int32_t> user_msg_count_;
  // private chat, can't ban user but can ignore
  std::unordered_map<UserId, UnixTime> blacklist_;
  mutable std::mutex mut_spam_;
  // some statistic measurements
  int32_t visitors_count_{0};
  int32_t dialog_used_{0};

  void AppendCommandToMenu(const std::string& cmd, const std::string& desc);
  bool IsUserBlocked(UserId user_id) const;
  void SendPost(const MsgPtr& msg, const content::Post& post) const;
  void SendPage(const MsgPtr& msg, const content::Page& page) const;

  std::string DownloadFileById(const std::string& file_id) const;
  // get the newest user avatars
  void ReceiveUserAvatars(const MsgPtr& msg, size_t max_count) const;

  void StartDialog(const CallbackPtr& cb, const content::Dialog& dialog);
  void EndDialog(const CallbackPtr& cb);
  // logging all user messages (text, emoji, forwarded)
  bool ReceiveMessages(const MsgPtr& msg, const std::string& label) const;
  // compressed by telegram, always jpeg
  bool ReceivePhotos(const MsgPtr& msg) const;
  // uncompressed or custom type, has mime
  bool ReceiveDocuments(const MsgPtr& msg) const;
  // user send -> process -> bot answer
  bool ProcessMessage(const MsgPtr& msg,
                      const content::Dialog::Entry& entry) const;
  void ProcessDialogs(const MsgPtr& msg);

  // background services
  using MemFn = void (Bot::*)();
  // background service, fire every N seconds
  void StartService(MemFn mem_fn, int32_t every_n_sec);
  void ServiceWriteStats();
  void ServiceSpamFilter();
  void ServiceDialogCleaner();
  const char* kParseMode{""};
};