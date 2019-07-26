*ssh-pageant-wrap*

Elementary wrap around ssh client from "Git for Windows" to interact with
PuTTY's pageant ssh-agent.

Licensed under WTFPL (see LICENSE).
I.e it's absolutely free and requires no permissions from anybody to Do What
~~The Fuck~~ You Want To. The author provides NO WARRANTY at all.

Some code of [weasel-ssh](https://github.com/vuori/weasel-pageant) is used,
thanks to Valtteri Vuorikoski, who distributes it under GNU GPLv3.

**Install**

The application is written on C++11 and depends on standard libraries and WinAPI
only. CMake >3.2 is used to configure build environment. But actually it's simple
enough to build manually.
