## AirDrop Telegram Bot CMS
Cross-platform YAML based CMS that allows you to easily build simple Telegram Bot.

- Fast. Small size. Portable.
- Receive and store files, messages from users.
- Detailed errors output for yaml.
- Ignore spammers. Erase stale dialogs.

![Telegram Bot](./docs/telegram-bot.png 'Telegram Bot')

## How to use

config.yaml:
- token (required)
- name (optional)
- username (optional)
- spam filter settings
- dialog settings
- log user statistics
See example ./bin/config.yaml

content.yaml:
- files (media or markdown)
- buttons
- markups
- start_page
- pages
- dialogs
See example ./bin/content.yaml

Put the media and markdown files into ./bin/content folder.

## Building

Install [vcpkg](https://github.com/microsoft/vcpkg).

```Console
cd AirDrop-Telegram-Bot-CMS
cmake -B build -S . --preset release-x64-linux
```

Reccomended to use VSCode with CMake Tools extension.

## TODO

- Database support, MySQL or SQLite.
- Check message size.

## Dependencies

Logger: [spdlog](https://github.com/gabime/spdlog)  
Unicode: [uni-algo](https://github.com/uni-algo/uni-algo)  
Telegram Bot: [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp)  
YAML: [yaml-cpp](https://github.com/jbeder/yaml-cpp)
