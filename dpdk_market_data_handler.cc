#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include <cstdio>
#include <cstdlib>

#include "Level2_order_book.h"
#include "read_big_endian_number.h"

#include <unistd.h>
#include <optional>
#include <fcntl.h>

constexpr uint16_t RX_RING_SIZE = 1024;
constexpr uint16_t NUM_MBUFS = 8191;
constexpr uint16_t MBUF_CACHE_SIZE = 250;
constexpr uint16_t BURST_SIZE = 32;

static Level2_Order_Book L2_BOOK_AAPL("AAPL",1000);
constexpr const char* TARGET_SYMBOL = "AAPL";

uint64_t count_A = 0;
uint64_t count_F = 0;
uint64_t count_E = 0;
uint64_t count_C = 0;
uint64_t count_X = 0;
uint64_t count_D = 0;
uint64_t count_U = 0;

std::optional<uint16_t> target_locate;
std::optional<uint64_t> expected_seq;


uint64_t fragmented_packets = 0;
uint64_t malformed_packets  = 0;
uint64_t sequence_gaps      = 0;

bool synchronized = false;


constexpr uint16_t IPV4_MF          = 0x2000;
constexpr uint16_t IPV4_FRAG_OFFSET = 0x1FFF;

#pragma pack(push, 1)

struct MoldUDP64Header
{
    char     session[10];
    uint64_t sequence_number;
    uint16_t message_count;
};

#pragma pack(pop)


struct BufferedPacket
{
    
    



};


std::map<uint64_t, BufferedPacket> pending;

static bool read_exact(int fd, void* dst, size_t len)
{
    auto* p = static_cast<uint8_t*>(dst);

    size_t done = 0;

    while (done < len) {
        size_t n = ::read(fd, p + done, len - done);

        if (n == 0)
            return false;

        if (n < 0) {
            perror("read");
            return false;
        }

        done += static_cast<size_t>(n);
    }

    return true;
}

static std::string read_stock(const uint8_t* p)
{
    std::string s(reinterpret_cast<const char*>(p), 8);

    while (!s.empty() && s.back() == ' ')
        s.pop_back();

    return s;
}


static void parse_stock_directory(
    const uint8_t* p,
    uint16_t len,
    std::optional<uint16_t>& target_locate)
{
    if (len != 39) {
        std::cerr << "Bad R length: " << len << '\n';
        return;
    }

    std::string stock = read_stock(p + 11);

    if (stock == TARGET_SYMBOL) 
    {
        std::cout << "AAPL R message:\n";

        for (int i = 0; i < 20; ++i) {
            printf("%02X ", p[i]);
        }

        printf("\n");

        target_locate = read_be::read_be16(p + 1);

        std::cout
            << "Found AAPL, stock locate = "
            << *target_locate
            << '\n';
    }

    return ;
}

static void parse_ITCH_message(
    const uint8_t* p,
    uint16_t len,
    std::optional<uint16_t>& target_locate)
{
    if (len == 0)
        return;

    char type = static_cast<char>(p[0]);

    // R 消息必须先看，因为我们靠它找 AAPL locate
    if (type == 'R' && !target_locate.has_value()) {

        parse_stock_directory(
            p,
            len,
            target_locate
        );

        return;
    }

    // AAPL locate 还没找到
    if (!target_locate.has_value()) {
        return;
    }

    if (len < 11) {
        std::cerr << "ITCH message shorter than common header: " << len << '\n';
        return;
    }

    // ITCH common header 中：
    // p[1], p[2] = stock_locate
    uint16_t locate = read_be::read_be16(p + 1);

    // 不是 AAPL，直接丢掉
    if (locate != *target_locate) {
        return;
    }

switch (type) {
    case 'A':
    count_A++;
    L2_BOOK_AAPL.Add_order(p);
    break;

case 'F':
    count_F++;
    L2_BOOK_AAPL.Add_order(p);
    break;

case 'E':
    count_E++;
    L2_BOOK_AAPL.Execute_order(p);
    break;

case 'C':
    count_C++;
    L2_BOOK_AAPL.Execute_order(p);
    break;

case 'X':
    count_X++;
    L2_BOOK_AAPL.Cancel_order(p);
    break;

case 'D':
    count_D++;
    L2_BOOK_AAPL.Delete_order(p);
    break;

case 'U':
    count_U++;
    L2_BOOK_AAPL.Replace_order(p);
    break;
}
}


bool parse_moldUDP64(const uint8_t *data, size_t len)
{

    if(len < 20)
    {
        return false;
    }

    const uint8_t *p = data;
    const uint8_t *end = p + len;
    
    uint64_t seq = read_be::read_be64(data + 10);
    uint16_t count = read_be::read_be16(p + 18);

    if(!expected_seq.has_value())
    {
        expected_seq = seq;
    }
    else if(expected_seq != seq)
    {
        synchronized = false;


    }
    else
    {
        synchronized = true;

    }

    if(count == 0 || count == 0xFFFF)
    {
        return true;
    }

    p += 20;

    for(uint16_t i = 0; i < count;i++)
    {
        if(p + 2 > end)
        {
            malformed_packets++;
            return false;
        }

        uint16_t msg_len = read_be::read_be16(p);

        p += 2;

        if(p + msg_len > end)
        {
            malformed_packets++;
            return false;
        }

        parse_ITCH_message(p,msg_len,target_locate);

        p += msg_len;
    }

    return true;
}


int main(int argc, char* argv[])
{
    int ret = rte_eal_init(argc, argv);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "EAL init failed\n");
    }

    uint16_t nb_ports = rte_eth_dev_count_avail();

    printf("Available ports: %u\n", nb_ports);

    if (nb_ports == 0) {
        rte_exit(EXIT_FAILURE, "No Ethernet ports found\n");
    }

    rte_mempool* mbuf_pool =
        rte_pktmbuf_pool_create(
            "MBUF_POOL",
            NUM_MBUFS,
            MBUF_CACHE_SIZE,
            0,
            RTE_MBUF_DEFAULT_BUF_SIZE,
            rte_socket_id()
        );

    if (mbuf_pool == nullptr) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }

    uint16_t port_id = 0;

    rte_eth_conf port_conf{};

    ret = rte_eth_dev_configure(
        port_id,
        1,   // RX queues
        0,   // TX queues
        &port_conf
    );

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot configure port\n");
    }

    ret = rte_eth_rx_queue_setup(
        port_id,
        0,
        RX_RING_SIZE,
        rte_eth_dev_socket_id(port_id),
        nullptr,
        mbuf_pool
    );

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot setup RX queue\n");
    }

    ret = rte_eth_dev_start(port_id);

    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot start port\n");
    }

    printf("DPDK RX started\n");

    rte_mbuf* packets[BURST_SIZE];

    while (true)
    {
        uint16_t nb_rx =
            rte_eth_rx_burst(
                port_id,
                0,
                packets,
                BURST_SIZE
            );

        for (uint16_t i = 0; i < nb_rx; ++i)
        {
            rte_mbuf* m = packets[i];

            if (m->nb_segs != 1)
            {
                rte_pktmbuf_free(m);
                continue;
            }

            uint8_t* frame =
                rte_pktmbuf_mtod(m, uint8_t*);

            size_t len =
                rte_pktmbuf_pkt_len(m);

            uint8_t* end =
                frame + len;

            // Ethernet
            if (len < sizeof(rte_ether_hdr))
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            auto* eth = reinterpret_cast<rte_ether_hdr*>(frame);

            if (eth->ether_type !=
                rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            // IPv4
            uint8_t* ip_ptr =
                frame + sizeof(rte_ether_hdr);

            if (ip_ptr + sizeof(rte_ipv4_hdr) > end)
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            auto* ip = reinterpret_cast<rte_ipv4_hdr*>(ip_ptr);

            uint16_t frag = rte_be_to_cpu_16(ip->fragment_offset);

            if ((frag & 0x2000) ||
                (frag & 0x1FFF))
            {
                ++fragmented_packets;

                rte_pktmbuf_free(m);

                continue;
            }

            uint16_t ip_header_len =
                (ip->version_ihl & 0x0F) * 4;

            if (ip_header_len < sizeof(rte_ipv4_hdr) ||
                ip_ptr + ip_header_len > end)
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            if (ip->next_proto_id != IPPROTO_UDP)
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            // UDP
            uint8_t* udp_ptr =
                ip_ptr + ip_header_len;

            if (udp_ptr + sizeof(rte_udp_hdr) > end)
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            auto* udp =
                reinterpret_cast<rte_udp_hdr*>(udp_ptr);

            uint16_t udp_len =
                rte_be_to_cpu_16(udp->dgram_len);

            if (udp_len < sizeof(rte_udp_hdr))
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            uint8_t* mold =
                udp_ptr + sizeof(rte_udp_hdr);

            size_t mold_len =
                udp_len - sizeof(rte_udp_hdr);

            if (mold + mold_len > end)
            {
                malformed_packets++;
                rte_pktmbuf_free(m);
                continue;
            }

            if (!parse_moldUDP64(mold, mold_len))
            {
                std::cerr << "Bad MoldUDP64 packet\n";
            }

            rte_pktmbuf_free(m);
        }
    }
    return 0;
}

