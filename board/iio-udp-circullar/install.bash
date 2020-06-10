sudo sysctl -w net.core.rmem_max=50331648;
sudo sysctl -w net.core.rmem_default=50331648;
sudo sysctl -w net.core.wmem_max=50331648;
sudo sysctl -w net.core.wmem_default=50331648;
sudo sysctl -w net.ipv4.udp_mem='17565 87380 50331648';
