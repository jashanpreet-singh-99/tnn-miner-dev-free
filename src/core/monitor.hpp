#pragma once

#include <tnn-common.hpp>
#include <terminal.h>

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>


namespace fs = std::filesystem;

void serve_monitor_framework(unsigned short port);

void m_signal_handler(int signal);