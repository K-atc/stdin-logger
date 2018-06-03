STDIN Logger
====

`stdin-logger` is a wrapper command to log stdin. This app is useful for CTF scene.


Build & Install
----
```
make && sudo make install
```

Usage
----
```
K_atc% stdin-logger 
usage: stdin-logger TRACEE [TRACEE args...]
```

### example
#### /bin/cat
```
K_atc% stdin-logger cat
test
test
google
google
github
github
K_atc% cat cat.2018-06-03-150154.stdin.log 
test
google
github
K_atc% 
```

#### a pwn problem
```
K_atc% stdin-logger ./notes
Exploit Me!!

---- [menu] ----
n: new note
u: update note
s: show notes
q: quit
input command: n

==== [note #0] ====
title: title
content: content

---- [menu] ----
n: new note
u: update note
s: show notes
q: quit
input command: s
note #0:title
    content

---- [menu] ----
n: new note
u: update note
s: show notes
q: quit
input command: q
Bye.
K_atc% cat notes.2018-06-03-150031.stdin.log 
n
title
content
s
q
K_atc% 
```