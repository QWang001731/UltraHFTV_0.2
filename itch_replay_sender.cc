#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kMaxUdpPayload = 1400;
constexpr std::size_t kBatchSize = 32;
constexpr uint16_t kDefaultDstPort = 5000;

// Replay datagram format:
// [ReplayHeader][msg_len_be16][ITCH msg][msg_len_be16][ITCH msg]...
#pragma pack(push, 1)
struct ReplayHeader {
    uint64_t seq_be;
    uint16_t msg_count_be;
    uint16_t payload_len_be;
};
#pragma pack(pop)

static_assert(sizeof(ReplayHeader) == 12, "ReplayHeader must be 12 bytes");

struct PacketBuffer {
    std::array<uint8_t, kMaxUdpPayload> data{};
    std::size_t len = 0;
    uint16_t msg_count = 0;
};

uint16_t read_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                 static_cast<uint16_t>(p[1]));
}

void init_packet(PacketBuffer& pkt, uint64_t seq) {
    pkt.len = sizeof(ReplayHeader);
    pkt.msg_count = 0;

    auto* hdr = reinterpret_cast<ReplayHeader*>(pkt.data.data());
    hdr->seq_be = htobe64(seq);
    hdr->msg_count_be = 0;
    hdr->payload_len_be = 0;
}

bool can_fit(const PacketBuffer& pkt, uint16_t msg_len) {
    return pkt.len + sizeof(uint16_t) + msg_len <= pkt.data.size();
}

void append_message(PacketBuffer& pkt, const uint8_t* msg, uint16_t msg_len) {
    const uint16_t len_be = htons(msg_len);

    std::memcpy(pkt.data.data() + pkt.len, &len_be, sizeof(len_be));
    pkt.len += sizeof(len_be);

    std::memcpy(pkt.data.data() + pkt.len, msg, msg_len);
    pkt.len += msg_len;

    ++pkt.msg_count;
}

void finalize_packet(PacketBuffer& pkt) {
    auto* hdr = reinterpret_cast<ReplayHeader*>(pkt.data.data());
    hdr->msg_count_be = htons(pkt.msg_count);

    const std::size_t payload_bytes = pkt.len - sizeof(ReplayHeader);
    if (payload_bytes > UINT16_MAX) {
        throw std::runtime_error("payload too large");
    }
    hdr->payload_len_be = htons(static_cast<uint16_t>(payload_bytes));
}

void usage(const char* prog) {
    std::cerr
        << "Usage: " << prog
        << " <itch_file> <dst_ip> [dst_port] [src_ip]\n\n"
        << "Example:\n"
        << "  " << prog
        << " 01302019.NASDAQ_ITCH50 10.10.10.2 5000 10.10.10.1\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) {
        usage(argv[0]);
        return 1;
    }

    const std::string itch_path = argv[1];
    const std::string dst_ip = argv[2];
    const uint16_t dst_port =
        (argc >= 4) ? static_cast<uint16_t>(std::stoi(argv[3]))
                    : kDefaultDstPort;
    const std::string src_ip = (argc >= 5) ? argv[4] : "";

    int file_fd = open(itch_path.c_str(), O_RDONLY);
    if (file_fd < 0) {
        std::perror("open");
        return 1;
    }

    struct stat st {};
    if (fstat(file_fd, &st) != 0) {
        std::perror("fstat");
        close(file_fd);
        return 1;
    }

    if (st.st_size <= 0) {
        std::cerr << "ITCH file is empty\n";
        close(file_fd);
        return 1;
    }

    auto* file_data = static_cast<uint8_t*>(
        mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0));

    if (file_data == MAP_FAILED) {
        std::perror("mmap");
        close(file_fd);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::perror("socket");
        munmap(file_data, st.st_size);
        close(file_fd);
        return 1;
    }

    if (!src_ip.empty()) {
        sockaddr_in src {};
        src.sin_family = AF_INET;
        src.sin_port = htons(0);

        if (inet_pton(AF_INET, src_ip.c_str(), &src.sin_addr) != 1) {
            std::cerr << "Invalid src_ip: " << src_ip << '\n';
            return 1;
        }

        if (bind(sock,
                 reinterpret_cast<sockaddr*>(&src),
                 sizeof(src)) != 0) {
            std::perror("bind");
            return 1;
        }
    }

    sockaddr_in dst {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(dst_port);

    if (inet_pton(AF_INET, dst_ip.c_str(), &dst.sin_addr) != 1) {
        std::cerr << "Invalid dst_ip: " << dst_ip << '\n';
        return 1;
    }

    const uint8_t* p = file_data;
    const uint8_t* end = file_data + st.st_size;

    uint64_t seq = 1;
    uint64_t total_packets = 0;
    uint64_t total_messages = 0;

    std::array<PacketBuffer, kBatchSize> batch_buffers{};
    std::array<mmsghdr, kBatchSize> msgs{};
    std::array<iovec, kBatchSize> iovs{};

    auto flush_batch = [&](std::size_t count) {
        if (count == 0) return;

        for (std::size_t i = 0; i < count; ++i) {
            iovs[i].iov_base = batch_buffers[i].data.data();
            iovs[i].iov_len = batch_buffers[i].len;

            std::memset(&msgs[i], 0, sizeof(msgs[i]));
            msgs[i].msg_hdr.msg_name = &dst;
            msgs[i].msg_hdr.msg_namelen = sizeof(dst);
            msgs[i].msg_hdr.msg_iov = &iovs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }

        std::size_t sent_total = 0;
        while (sent_total < count) {
            int rc = sendmmsg(sock,
                              msgs.data() + sent_total,
                              static_cast<unsigned int>(count - sent_total),
                              0);
            if (rc < 0) {
                if (errno == EINTR) continue;
                std::perror("sendmmsg");
                throw std::runtime_error("sendmmsg failed");
            }
            sent_total += static_cast<std::size_t>(rc);
        }

        total_packets += count;
    };

    std::size_t batch_count = 0;

    try {
        while (p < end) {
            PacketBuffer& pkt = batch_buffers[batch_count];
            init_packet(pkt, seq);

            while (p < end) {
                if (end - p < 2) {
                    throw std::runtime_error("truncated 2-byte message length");
                }

                const uint16_t msg_len = read_be16(p);
                p += 2;

                if (msg_len == 0) {
                    throw std::runtime_error("zero-length ITCH message");
                }

                if (end - p < msg_len) {
                    throw std::runtime_error("truncated ITCH message");
                }

                if (!can_fit(pkt, msg_len)) {
                    // Roll back length prefix so next packet can consume it.
                    p -= 2;

                    if (pkt.msg_count == 0) {
                        throw std::runtime_error(
                            "single ITCH message exceeds UDP payload limit");
                    }
                    break;
                }

                append_message(pkt, p, msg_len);
                p += msg_len;
                ++total_messages;
            }

            finalize_packet(pkt);
            ++seq;
            ++batch_count;

            if (batch_count == kBatchSize) {
                flush_batch(batch_count);
                batch_count = 0;
            }
        }

        flush_batch(batch_count);
    } catch (const std::exception& e) {
        std::cerr << "Replay failed: " << e.what() << '\n';
        close(sock);
        munmap(file_data, st.st_size);
        close(file_fd);
        return 1;
    }

    std::cout
        << "Replay complete\n"
        << "Packets sent : " << total_packets << '\n'
        << "ITCH messages: " << total_messages << '\n'
        << "Last seq     : " << (seq - 1) << '\n';

    close(sock);
    munmap(file_data, st.st_size);
    close(file_fd);
    return 0;
}
