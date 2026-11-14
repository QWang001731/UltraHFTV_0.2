g++ -std=c++17 -O3 -march=native -Wall -Wextra \
  dpdk_market_data_handler.cc \
  -o dpdk_market_data_handler \
  $(pkg-config --cflags --libs libdpdk)


g++ -std=c++17 -O3 -march=native -Wall -Wextra \
itch_replay_sender.cc -o itch_replay_sender