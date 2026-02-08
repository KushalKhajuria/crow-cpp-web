# Crow C++ Web App

A simple C++ web server built with Crow that serves a Tailwind-styled frontend
and exposes JSON APIs.

## Features
- C++ backend (Crow)
- Tailwind CSS frontend
- GET + POST APIs
- In-memory processing

## Run locally
```bash
clang++ -std=c++17 src/main.cpp \
  -I$(brew --prefix crow)/include \
  -I$(brew --prefix asio)/include \
  -o app

./app
