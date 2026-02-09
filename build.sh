#!/bin/bash
clang++ -std=c++17 \
  src/main.cpp \
  src/authentication/auth.cpp \
  src/number_reverser/number_reverser.cpp \
  src/othello/othello.cpp \
  -Isrc/authentication \
  -Isrc/number_reverser \
  -Isrc/othello \
  -I$(brew --prefix crow)/include \
  -I$(brew --prefix asio)/include \
  -I$(brew --prefix libsodium)/include \
  -I$(brew --prefix sqlite)/include \
  -L$(brew --prefix libsodium)/lib \
  -L$(brew --prefix sqlite)/lib \
  -lsodium -lsqlite3 \
  -o app
