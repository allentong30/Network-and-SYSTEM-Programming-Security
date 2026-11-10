# 封包錄製與分析指南（待使用者操作）

## 同一台 WSL / Linux：tcpdump

先啟動 `./build/1/server --debug`，另一個終端機錄製：

```bash
sudo apt-get install tcpdump
sudo tcpdump -i lo -nn -s 0 -w sysprobe-loopback.pcap 'tcp port 9090'
```

第三個終端機：

```bash
./build/1/client 127.0.0.1 9090 SYSINFO
./build/1/client 127.0.0.1 9090 MEMORY
./build/1/client 127.0.0.1 9090 DISK
./build/1/client 127.0.0.1 9090 PING
```

tcpdump 按 Ctrl+C 停止，再檢視：

```bash
tcpdump -nn -r sysprobe-loopback.pcap -X
```

可用 Wireshark 開啟此 pcap，以 `tcp.port == 9090` 篩選。WSL 的 loopback 與 Windows loopback 不一定相同，使用上述 WSL tcpdump 最直接。

## 跨電腦：Wireshark

Server 使用 `./build/1/server --bind 0.0.0.0 --port 9090 --debug`，client 使用 server 可連到的 IPv4。選擇實際通訊網卡，capture filter `tcp port 9090`。先確認兩台機器可連線；若使用 WSL NAT，需按實際網路環境處理轉送。

## 報告應分析的內容

1. 寫明實際 client/server IP、port、介面、時間與指令。
2. 標出 SYN → SYN/ACK → ACK 三向交握，及 FIN/ACK 或 RST 結束方式；依實際擷取判讀。
3. 找到 SYSINFO request：應用層 bytes 為 `00 00 00 07 53 59 53 49 4e 46 4f`。前 4 bytes 為 network byte order 的長度 7，後面是 ASCII SYSINFO。
4. 找到 server `OK host=...` 回覆，解析它的 4-byte 長度與文字，核對程式輸出。
5. 使用 Follow TCP Stream 檢查重組結果。TCP packet 邊界與應用層 frame 邊界無一對一關係：header/body 可能拆包或合併。
6. 標出 sequence/acknowledgment numbers、payload length 和查詢往返時間。若看不到重傳，不要聲稱有重傳。
7. 可另錄 `make test` 的異常比較，但測試使用動態 port，capture filter 需調整為測試實際 port 或在隔離測試環境暫不設 port filter。

建議繳交：`sysprobe-loopback.pcap`、握手截圖、request hex 截圖、Follow TCP Stream 截圖與一頁逐項分析。pcap 預設不納入 Git，避免混入無關流量；若要上傳，先確認只含這次作業。
