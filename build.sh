#!/bin/bash
g++ -std=c++17 -Wall -Wextra -pthread main.cpp ringbuffer.cpp
sudo systemctl restart daq_server.service
