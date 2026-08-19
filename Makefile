# Network Configuration Variables
BRIDGE          := bridge0
CLIENT_NS       := client
SERVER_NS       := server
WAN_IF          := wlp43s0

# Subnet & IP Assignments
PHYS_SUBNET     := 10.1.1.0/24
BRIDGE_IP       := 10.1.1.1/24
CLIENT_PHYS_IP  := 10.1.1.10/24
SERVER_PHYS_IP  := 10.1.1.20/24
SERVER_PHYS_GW  := 10.1.1.1

VPN_SUBNET      := 10.8.0.0/24
VPN_CLIENT_IP   := 10.8.0.2/24
VPN_SERVER_IP   := 10.8.0.1/24
VPN_SERVER_GW   := 10.8.0.1

# Define netns DNS directory variable
NETNS_CLIENT_DIR := /etc/netns/$(CLIENT_NS)

# Compilation Variables
CC              := gcc
CFLAGS          := -Wall -Wextra -I include
LDLIBS          := -lsodium
SRC_DIR         := src
BUILD_DIR       := build

CLIENT_BIN      := $(BUILD_DIR)/client
SERVER_BIN      := $(BUILD_DIR)/server
KEYGEN_BIN      := $(BUILD_DIR)/keygen


.PHONY: all setup setup-net setup-tun run-server run-client keygen clean teardown

all: $(CLIENT_BIN) $(SERVER_BIN) setup

# 1. C Compilation

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(CLIENT_BIN): $(SRC_DIR)/vpn_client.c $(SRC_DIR)/tun.c $(SRC_DIR)/socket.c $(SRC_DIR)/config.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(SERVER_BIN): $(SRC_DIR)/vpn_server.c $(SRC_DIR)/tun.c $(SRC_DIR)/socket.c $(SRC_DIR)/config.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(KEYGEN_BIN): $(SRC_DIR)/keygen.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# 2. Network Environment Setup
setup: setup-net

setup-net:
	@echo "[+] Creating network namespaces..."
	sudo ip netns add $(CLIENT_NS)
	sudo ip netns add $(SERVER_NS)

	@echo "[+] Creating bridge and veth pairs..."
	sudo ip link add $(BRIDGE) type bridge
	sudo ip link set $(BRIDGE) up
	sudo ip link add veth_client type veth peer name veth_client_br0
	sudo ip link add veth_server type veth peer name veth_server_br0

	@echo "[+] Attaching veth interfaces to namespaces and bridge..."
	sudo ip link set veth_client netns $(CLIENT_NS)
	sudo ip link set veth_client_br0 master $(BRIDGE)
	sudo ip link set veth_server netns $(SERVER_NS)
	sudo ip link set veth_server_br0 master $(BRIDGE)

	@echo "[+] Bringing up physical layer interfaces..."
	sudo ip netns exec $(CLIENT_NS) ip link set veth_client up
	sudo ip link set veth_client_br0 up
	sudo ip netns exec $(SERVER_NS) ip link set veth_server up
	sudo ip link set veth_server_br0 up

	@echo "[+] Assigning IP addresses..."
	sudo ip netns exec $(CLIENT_NS) ip addr add $(CLIENT_PHYS_IP) dev veth_client
	sudo ip netns exec $(SERVER_NS) ip addr add $(SERVER_PHYS_IP) dev veth_server
	sudo ip addr add $(BRIDGE_IP) dev $(BRIDGE)

	@echo "[+] Configuring server forwarding, default routes, and NAT..."
	# Enable IP forwarding on host and inside server namespace
	sudo sysctl -w net.ipv4.ip_forward=1
	sudo ip netns exec $(SERVER_NS) sysctl -w net.ipv4.ip_forward=1

	# Default route for server namespace out to host bridge
	sudo ip netns exec $(SERVER_NS) ip route add default via $(SERVER_PHYS_GW)

	# 1. NAT inside server NS: Translates VPN subnet traffic (10.8.0.0/24) leaving veth_server
	sudo ip netns exec $(SERVER_NS) iptables -t nat -A POSTROUTING -s $(VPN_SUBNET) -o veth_server -j MASQUERADE
	sudo ip netns exec $(SERVER_NS) iptables-legacy -t nat -A POSTROUTING -s 10.8.0.0/24 -o veth_server -j MASQUERADE
	sudo ip netns exec $(SERVER_NS) iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -j SNAT --to-source 10.1.1.20
    
	# 2. NAT on host: Translates bridge traffic (10.1.1.0/24) out to WAN (wlp43s0)
	sudo iptables -t nat -A POSTROUTING -s $(PHYS_SUBNET) -o $(WAN_IF) -j MASQUERADE

	@echo "[+] Configuring client namespace DNS..."
	sudo mkdir -p /etc/netns/$(CLIENT_NS)
	echo "nameserver 8.8.8.8" | sudo tee /etc/netns/$(CLIENT_NS)/resolv.conf > /dev/null

	@echo "[+] Network topology setup complete!"

# 3. Post-Launch TUN Interface Configuration
# Execute 'make setup-tun' AFTER launching client and server applications
setup-tun:
	@echo "[+] Configuring TUN interfaces..."
	sudo ip netns exec $(CLIENT_NS) ip addr add $(VPN_CLIENT_IP) dev tun0
	sudo ip netns exec $(CLIENT_NS) ip link set dev tun0 up

	sudo ip netns exec $(SERVER_NS) ip addr add $(VPN_SERVER_IP) dev tun1
	sudo ip netns exec $(SERVER_NS) ip link set dev tun1 up

	@echo "[+] Setting up VPN client default route without breaking UDP encapsulation..."
	# Direct host route for physical server endpoint via veth_client
	sudo ip netns exec $(CLIENT_NS) ip route add 10.1.1.20 via $(SERVER_PHYS_GW) dev veth_client || true
	# Tunnel default route
	sudo ip netns exec $(CLIENT_NS) ip route replace default via $(VPN_SERVER_GW) dev tun0
	@echo "[+] TUN interfaces configured!"

# 4. Execution Shortcuts
run-server: $(SERVER_BIN)
	sudo ip netns exec $(SERVER_NS) ./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	sudo ip netns exec $(CLIENT_NS) ./$(CLIENT_BIN)

keygen: $(KEYGEN_BIN)
	./$(KEYGEN_BIN) $(TARGET)

# 5. Teardown & Clean
teardown:
	@echo "[-] Removing client namespace DNS configuration..."
	-sudo rm -rf /etc/netns/$(CLIENT_NS) 2>/dev/null || true
	@echo "[-] Removing iptables NAT rules..."
	-sudo ip netns exec $(SERVER_NS) iptables -t nat -D POSTROUTING -s $(VPN_SUBNET) -o veth_server -j MASQUERADE 2>/dev/null || true
	-sudo ip netns exec $(SERVER_NS) iptables-legacy -t nat -D POSTROUTING -s 10.8.0.0/24 -o veth_server -j MASQUERADE 2>/dev/null || true
	-sudo ip netns exec $(SERVER_NS) iptables -t nat -D POSTROUTING -s 10.8.0.0/24 -j SNAT --to-source 10.1.1.20 2>/dev/null || true
	-sudo iptables -t nat -D POSTROUTING -s $(PHYS_SUBNET) -o $(WAN_IF) -j MASQUERADE 2>/dev/null || true
	@echo "[-] Destroying network namespaces and bridge..."
	-sudo ip netns del $(CLIENT_NS) 2>/dev/null || true
	-sudo ip netns del $(SERVER_NS) 2>/dev/null || true
	-sudo ip link del $(BRIDGE) 2>/dev/null || true
	@echo "[-] Teardown complete!"

clean: teardown	