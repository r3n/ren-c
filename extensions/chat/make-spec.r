REBOL []

name: 'Chat
source: %chat/mod-chat.c
includes: copy [
    %prep/extensions/chat ;for %tmp-extensions-chat-init.inc
]

libraries: _

options: []
