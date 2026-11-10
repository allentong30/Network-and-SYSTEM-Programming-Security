# SysProbe：遠端系統資訊與健康檢查服務

Linux / C 期中專題。TCP server 使用 `fork()`，每個連線一個子行程，不使用執行緒。已在 WSL Ubuntu 完成 12 個同時連線與三組異常比較，詳見 [報告](docs/REPORT.md) 和 [實測結果](evidence/results.json)。

## 建置與執行

在 Ubuntu / WSL 終端機操作（不是 Windows 原生 gcc）：

```bash
cd /mnt/c/examcode/midterm-sysprobe
# 若從 GitHub clone，請 cd 到 clone 下的 midterm-sysprobe
sudo apt-get update
sudo apt-get install build-essential python3
make
./build/1/server --debug
```

另一個終端機：

```bash
./build/1/client 127.0.0.1 9090 SYSINFO --debug
./build/1/client 127.0.0.1 9090 MEMORY
./build/1/client 127.0.0.1 9090 DISK
./build/1/client 127.0.0.1 9090 PING
./build/1/client 127.0.0.1 9090 'HOLD 3'
```

server 按 Ctrl+C 結束；父行程關閉 listener、終止子行程並 waitpid 回收。每個連線可送多個 framed requests；CLI client 每次執行送一筆。HOLD 是受限 1–10 秒的並行展示功能。

## Debug 的兩個層級

```bash
make DEBUG=1                     # build/1：包含 debug 程式碼
./build/1/server --debug          # 顯示 DEBUG
./build/1/server                  # 不顯示 DEBUG
make DEBUG=0                     # build/0：編譯時移除 debug
./build/0/server --debug          # 仍不顯示 DEBUG
```

兩種 build 目錄分離，切換不用 clean。client 同樣支援 `--debug`；一般 INFO/錯誤訊息不受 debug 開關影響。兩個 server 範例請依序執行，避免同 port 衝突。

## 驗證

```bash
make test
ldd build/1/server
ldd build/1/client
```

測試會啟動 loopback server、執行 C client，並用 Python 注入 TCP 異常及同時開啟 12 個連線。`evidence/` 保存實際 process snapshot、動態連結結果、server logs 和 JSON 結果。測試會更新這些證據。Python 只負責測試，正式 client/server 與 shared library 全部以 C 實作。

## 三組 robustness 比較

`--baseline` 是刻意保留缺陷的教學比較模式，僅用於本機測試。它依然保留長度上限、fork 架構與父行程清理。

| 案例 | 預設 robust | `--baseline` |
|---|---|---|
| TCP body 分段到達 | 迴圈接收完整 body | 單次 recv 不足即斷線 |
| 只送 1 byte header 後停止 | 整個 frame 有絕對 deadline，釋放子行程 | MSG_WAITALL 無 deadline，持續占用子行程 |
| 未知命令後再送 PING | 回覆 ERR，可繼續使用相同連線 | parser 直接放棄該連線 |

其他保護：最大 4096 bytes、拒絕空 frame/NUL、處理 partial send/recv、EINTR/EAGAIN、MSG_NOSIGNAL、連線上限 64、socket/fork 失敗處理、waitpid 回收、重啟時 SO_REUSEADDR。

## 封包錄製與繳交

請依 [封包操作指南](docs/PACKET_CAPTURE.md) 自行錄製並補上實際分析。報告 [REPORT.md](docs/REPORT.md) 已包含設計、關鍵程式、比較結果、限制及截圖清單。提交前補姓名學號、自己的操作截圖和 pcap；不要把尚未錄製的分析寫成已完成。

預設只綁定 loopback。跨電腦展示可用 `--bind 0.0.0.0 --port 9090`，client 指向 server 的實際 IPv4；WSL 的 NAT/轉送依環境設定。本版為課堂明文 TCP 示範，沒有 TLS、認證或遠端 shell。磁碟查詢固定 `/`，不執行來自 client 的指令。

## 來源對照

參考使用者提供的 `C:\CODE`：`sockets/server/server4.c`（fork server）、`TLS-Socket/TLS-Socket/program1/server_plain.c`（TCP 流程）、`develop_debug/debug/debug_ultra_min.c`（compile/runtime debug）、`develop_debug/shared_library/README`（.so）、`signal4/Signal4.c`（signal 與回收）。本專題以這些概念重新實作；未複製範例二進位檔或 TLS 私鑰。
