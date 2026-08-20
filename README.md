# Linux L-3 Userspace VPN

A small point-to-point VPN prototype written in C for Linux. The client and server exchange IPv4 packets through TUN interfaces, encrypt the packets with libsodium, and transport them over UDP.

> **Status:** This is an educational/prototype implementation. Review and harden the networking, key management, packet validation, privilege handling, and error handling before using it for untrusted networks.

## Features

- Linux TUN interfaces (`tun0` on the client and `tun1` on the server)
- UDP transport on port `9999`
- Non-blocking sockets and `epoll`
- libsodium public-key session key exchange using `crypto_kx`
- ChaCha20-Poly1305-IETF authenticated encryption
- Optional network namespace, bridge, veth, forwarding, routing, and NAT automation through the Makefile
- Hex-encoded key files stored under `config/`

## Project layout

```text
.
├── include/       C headers
├── src/           Client, server, TUN, socket, configuration, and key-generation code
├── config/        Generated key files
├── build/         Compiled binaries
└── Makefile       Build and network namespace automation
```

## Requirements

The project currently targets Linux and requires:

- GCC
- GNU Make
- libsodium development files
- `iproute2` (`ip` command)
- `iptables` or `iptables-legacy`
- `/dev/net/tun`
- Root privileges for TUN and namespace setup

On Debian or Ubuntu, install the common dependencies with:

```sh
sudo apt install build-essential libsodium-dev iproute2 iptables
```

## Build

Run commands from the repository root:

```sh
make
```

The build produces:

- `build/client`
- `build/server`
- `build/keygen`

The default `make` target builds the client and server, then invokes `setup`, which is an alias for `setup-net`. Therefore, `make` creates the namespaces, bridge, veth pairs, routes, forwarding, NAT rules, and DNS configuration described below.

To build without changing the host network configuration, use:

```sh
make build/client build/server build/keygen
```

## Generate keys

Generate one key pair for each endpoint:

```sh
make keygen TARGET=client
make keygen TARGET=server
```

This creates:

```text
config/client_public.key
config/client_private.key
config/server_public.key
config/server_private.key
```

The programs expect these paths relative to the directory from which they are started. Keep private keys secret and do not commit them to version control.

## Network topology

The Makefile contains a test topology consisting of two network namespaces connected to a host bridge. The topology has two addressing layers.

### 1. Physical network

| Component | Address / value |
|---|---|
| Host bridge (`bridge0`) | `10.1.1.1/24` |
| Client veth (`veth_client`) | `10.1.1.10/24` |
| Server veth (`veth_server`) | `10.1.1.20/24` |

### 2. VPN tunnel

The project uses TUN devices:

| Component | Address / value |
|---|---|
| Server VPN interface (`tun1`) | `10.8.0.1/24` |
| Client VPN interface (`tun0`) | `10.8.0.2/24` |
| VPN network | `10.8.0.0/24` |
| UDP transport port | `9999` |

The physical network carries the encrypted UDP packets. The TUN interfaces carry the decrypted IP packets routed through the VPN.

Before using the optional namespace targets, check the `WAN_IF` value in the Makefile. It defaults to `wlp43s0` and must match the host's Internet-facing interface. The client currently connects to the hard-coded server endpoint `10.1.1.20:9999` in `src/vpn_client.c`.

## Run the VPN in the namespace test topology

1. Generate both key pairs.
2. Build the binaries and create the network topology:

   ```sh
   make
   ```

3. Start the server in one terminal:

   ```sh
   make run-server
   ```

4. Start the client in another terminal:

   ```sh
   make run-client
   ```

5. After both programs have created their TUN devices, configure the TUN interfaces and routes:

   ```sh
   make setup-tun
   ```

The client encrypts packets read from `tun0` and sends them to the server over UDP. The server decrypts them and writes them to `tun1`; traffic in the reverse direction follows the same process.


## Build or run without namespaces

If you do not want to create the namespace test topology, build only the binaries:

```sh
make build/client build/server build/keygen
```

The `run-server`, `run-client`, and `setup-tun` targets use the `client` and `server` namespaces, so they are only appropriate after `make` or `make setup-net`. For a non-namespace setup, run the binaries directly and configure the TUN interfaces and routes separately:

```sh
sudo ./build/server
sudo ./build/client
```

## Cleanup

Stop the client and server with `Ctrl+C`, then remove the namespaces, bridge, routes, DNS configuration, and NAT rules created by `make`:

```sh
make teardown
```

To clean the generated network state and rebuild artifacts, use:

```sh
make clean
```

## Configuration notes

Network values are defined at the top of the Makefile, including namespace names, interface names, physical addresses, VPN addresses, and the WAN interface. The UDP server address and port are currently defined directly in the C sources:

- Server binds to `0.0.0.0:9999`.
- Client connects to `10.1.1.20:9999`.

Changing these values requires editing the corresponding source files and rebuilding.
