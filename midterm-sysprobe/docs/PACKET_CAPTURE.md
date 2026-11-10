# Packet Capture and Analysis Guide

Packet capture and analysis are pending and will be completed by the student.

## Loopback on Linux or WSL: tcpdump

Start `./build/1/server --debug`. In a second terminal:

```bash
sudo apt-get install tcpdump
sudo tcpdump -i lo -nn -s 0 -w sysprobe-loopback.pcap 'tcp port 9090'
```

In a third terminal:

```bash
./build/1/client 127.0.0.1 9090 SYSINFO
./build/1/client 127.0.0.1 9090 MEMORY
./build/1/client 127.0.0.1 9090 DISK
./build/1/client 127.0.0.1 9090 PING
```

Stop tcpdump with Ctrl+C, then inspect:

```bash
tcpdump -nn -r sysprobe-loopback.pcap -X
```

Alternatively, open the pcap in Wireshark and apply `tcp.port == 9090`. WSL and Windows loopback may differ; capturing inside WSL avoids that ambiguity.

## Between computers: Wireshark

Run `./build/1/server --bind 0.0.0.0 --port 9090 --debug`. Connect the client to the server's reachable IPv4 address. Select the interface carrying the traffic and capture filter `tcp port 9090`. Verify connectivity first. WSL NAT may require forwarding appropriate to the environment.

## Analysis checklist

1. Record actual client/server IP addresses, ports, interface, capture time, and commands.
2. Identify the SYN -> SYN/ACK -> ACK handshake and observed FIN/ACK or RST termination.
3. Locate the SYSINFO request: `00 00 00 07 53 59 53 49 4e 46 4f`. The first four bytes encode length 7 in network order; the rest is ASCII SYSINFO.
4. Locate the `OK host=...` response, decode its length and text, and compare with client output.
5. Use Follow TCP Stream for reassembly. TCP packet boundaries do not map directly to application frames; headers and bodies may be split or combined.
6. Explain sequence/acknowledgment numbers, payload lengths, and request-response timing. Discuss retransmissions only if observed.
7. Optionally capture robustness tests. They use dynamic ports, so adjust the filter to the actual test port or omit the port filter in an isolated test environment.

Suggested deliverables: `sysprobe-loopback.pcap`, handshake screenshot, request hex screenshot, Follow TCP Stream screenshot, and a short analysis. Captures are ignored by Git by default. Before publishing one, verify that it contains only the intended project traffic.
