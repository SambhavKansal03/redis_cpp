#pragma once
#include "store/kvstore.h"

// Handles one client for its entire lifetime:
// reads commands in a loop, executes them, writes responses.
// When the client disconnects, this function returns.
void handle_client_connection(int client_fd, KVStore& store);