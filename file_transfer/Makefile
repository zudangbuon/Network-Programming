CXX=g++
CXXFLAGS=-std=c++17 -O2 -pthread -Iinclude

BIN_DIR=bin

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/server: $(BIN_DIR) src/server.cpp include/net.hpp include/rate_limiter.hpp include/sha256.hpp include/hash.hpp include/file_utils.hpp include/protocol.hpp include/transfer.hpp
	$(CXX) $(CXXFLAGS) src/server.cpp -o $(BIN_DIR)/server

$(BIN_DIR)/client: $(BIN_DIR) src/client.cpp include/net.hpp include/sha256.hpp include/hash.hpp include/protocol.hpp include/transfer.hpp include/load_test.hpp include/terminal_ui.hpp
	$(CXX) $(CXXFLAGS) src/client.cpp -o $(BIN_DIR)/client

clean:
	rm -rf $(BIN_DIR)
