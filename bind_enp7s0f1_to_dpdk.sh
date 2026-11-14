# 2. Bring the Linux interface down
sudo ip link set enp7s0f1 down

# 3. Load VFIO
sudo modprobe vfio-pci

# 4. Bind the NIC to DPDK
sudo dpdk-devbind.py --bind=vfio-pci 0000:07:00.1