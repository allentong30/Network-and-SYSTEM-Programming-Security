# 期中專題報告：SysProbe

姓名／學號：待填。測試日期：2026-11-10（Asia/Taipei）。

## 一、系統設計

本系統提供遠端主機健康資訊查詢。Linux C client 透過 IPv4 TCP 傳送命令，server 父行程以非阻塞 listener 配合 poll 接收連線，每次 accept 後 fork 一個專用子行程。子行程關閉 listener，父行程關閉 accepted fd；因此不同 client 不會由同一個行程循序服務。本系統不使用 threads。

```text
client ── TCP ── parent server (accept / fork / reap)
                 ├─ child A: client A requests
                 ├─ child B: client B requests
                 └─ ... up to 64 children
client ─┐
        ├─ dynamically link libsysprobe.so
server ─┘
```

指令包括 SYSINFO（uname 主機、OS、kernel、架構及 child PID）、MEMORY（sysinfo uptime、total/free RAM、1 分鐘 load）、DISK（statvfs 根目錄總空間與可用空間）、PING、HOLD 1–10 及 QUIT。free RAM 是核心直接提供的 free，不等於可回收 cache 後的 available；load 也不是 CPU 使用率。

協定：4-byte unsigned big-endian payload length + 1–4096 bytes 非 NUL ASCII 文字。response 同樣 framing，`OK ` 或 `ERR ` 開頭。每條連線允許連續 request；先收到完整 request 再送 response。HOLD 只供並行示範。

## 二、關鍵原始碼

| 檔案 | 主要責任 |
|---|---|
| `src/server.c` | accept/fork、64 人上限、命令分派、uname/sysinfo/statvfs、子行程追蹤與回收、停止清理 |
| `src/client.c` | CLI 解析、連線、送出 request、列印 response 與回傳狀態 |
| `src/common.c` | framed I/O、deadline、poll、partial I/O、DNS/連線、數字解析與 PID 日誌 |
| `include/common.h` | 共用介面與編譯／執行 debug macro |
| `Makefile` | PIC shared library、兩個 executable、ORIGIN rpath、debug/release 分目錄 |
| `tests/integration.py` | 真實 C client 測試、12 人並行、socket 異常注入及 evidence 保存 |

`libsysprobe.so` 由 `-fPIC -shared` 建立，兩邊都用 `-lsysprobe` 動態連結，`$ORIGIN` rpath 讓 executable 找同目錄的 library。`evidence/server-ldd.txt` 及 `client-ldd.txt` 可確認。日誌在 shared library 內實作，DEBUG macro 在 `make DEBUG=0` 時移除呼叫，執行時的 `--debug` 再控制已編入的 DEBUG 訊息。

## 三、異常與穩健性

每次 frame 使用 CLOCK_MONOTONIC 絕對 deadline，反覆 partial I/O 不會重置時間。poll 後以 MSG_DONTWAIT 實際收送，重試 EINTR/EAGAIN，send 使用 MSG_NOSIGNAL 避免斷線造成 SIGPIPE。header 長度先驗證才收 body，拒絕 NUL。父行程每 100ms poll 週期以 waitpid(WNOHANG) 清理子行程，避免 zombie 長期累積。SIGINT/SIGTERM handler 只設定停止旗標；父行程在正常流程停止 listener、終止和回收子行程。

`--baseline` 明確用於教學 ablation，比較的是同一系統拿掉某些機制後的行為；它沒有刻意加入 buffer overflow。比較實測：

| 測試 | Robust | Baseline |
|---|---|---|
| SYSINFO body 先送 1 byte，200ms 後補齊 | 收齊並成功回覆 | 單次 recv 不完整，關閉連線 |
| 只送 1 byte header，等待 1.1 秒 | 800ms deadline 後子行程已清除 | 子行程仍等待 header |
| INVALID 後在同連線送 PING | ERR 後可收到 OK PONG | 未知命令使連線關閉 |

這三組由同一支 integration test 依序對兩種模式執行，結果見 `evidence/results.json`。另驗證空／過長／NUL frame 被拒絕，後續新 client 仍可服務。`--baseline` 保留長度上限、父行程管理等其他保護，不能說是所有 robustness 全部關閉。

## 四、執行證明

WSL Ubuntu Linux 上 `make test` 完成 13 項 PASS。編譯啟用 `-Wall -Wextra -Wpedantic -Werror`。測試建立 12 條 TCP 連線並送 HOLD 3，0.3 秒後從 `/proc/<parent>/task/<parent>/children` 讀到 12 個存活 PID，以 `ps` 保存同時存在的父子關係，再收到 12 個不同 PID 回覆。這證明至少 10 人同時服務，不只是 listen backlog 或連線排隊。所有 client 關閉後再確認子行程清空。

實際 PID、時間、平台與輸出保存在 `evidence/results.json` 及 `concurrency-processes.txt`，請以檔案為準；重跑會產生不同 PID。測試腳本使用 Python socket 來精確分段送資料，並非 Python server 代替 C server。

## 五、執行截圖與封包

下圖為瀏覽器開啟真實測試紀錄頁的截圖，來源是上述實測 JSON 與 ps snapshot，並非即時終端機。原始頁面為 `evidence/execution.html`。重跑測試會更新文字證據；此圖為 2026-11-10 保存的版本，不會自動重新產生。

![實測結果與 12 個子行程紀錄](../evidence/execution.png)

現有 evidence 亦保留原始文字執行證據。若老師要求終端機操作過程畫面，請在自己的 Ubuntu 終端機重跑並截取：

1. `make` 成功、server 啟動與 client SYSINFO/MEMORY/DISK 輸出。
2. `make test` 的 12 concurrent clients PASS，以及 `cat evidence/concurrency-processes.txt` 的 12 組子行程。
3. robustness 三組 robust/baseline PASS 與對應結果。
4. DEBUG=1 + --debug、DEBUG=1 無 --debug、DEBUG=0 + --debug 的差異。
5. 兩邊 ldd 中的 `libsysprobe.so`。

封包由使用者自行錄製，步驟與分析問題見 [PACKET_CAPTURE.md](PACKET_CAPTURE.md)。目前沒有 pcap，不宣稱已完成 packet analysis。請加入實際抓到的握手、SYSINFO frame、response、結束連線畫面與分析。

## 六、限制與可擴充方向

此版本適用 Linux，含 sysinfo 與 `/proc` 測試工具。僅支援 IPv4；getaddrinfo 的 DNS 查詢時間不含於 TCP connect deadline。伺服器最多 64 個 child、每 frame 最多 4096 bytes，預設 I/O deadline 3 秒；HOLD 的處理時間另外計算。父行程日誌為盡力寫入，沒有持久化佇列。服務未提供認證／TLS，預設 loopback；跨主機僅在課堂可控環境展示。可擴充認證、TLS、JSON 回覆與 CPU 趨勢，但本次著重老師指定的 process、network、shared library 與 robustness。

## 七、完整原始碼

完整原始碼隨 repository 的 `src/`、`include/`、`Makefile` 和 `tests/` 提交。若老師要求單一文件，將上述檔案完整附於報告附錄；不可只交編譯後 executable。
