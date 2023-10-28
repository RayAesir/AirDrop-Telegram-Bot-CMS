// deps
#include <spdlog/spdlog.h>
// local
#include "bot.h"
#include "content.h"

int main() {
#if defined(_WIN32) || defined(_WIN64)
  // Enable UTF-8 console output
  SetConsoleOutputCP(CP_UTF8);
#endif

  // config and content
  if (!content::Initialize()) return -1;

  Bot bot;
  bot.LongPoll();

  return 0;
}