#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

#include <cstdio>
#include <cstdlib>

constexpr uint16_t RX_RING_SIZE = 1024;
constexpr uint16_t NUM_MBUFS = 8191;
constexpr uint16_t MBUF_CACHE_SIZE = 250;
constexpr uint16_t BURST_SIZE = 32;

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

            printf(
                "received packet, len=%u\n",
                rte_pktmbuf_pkt_len(m)
            );

            rte_pktmbuf_free(m);
        }
    }
}