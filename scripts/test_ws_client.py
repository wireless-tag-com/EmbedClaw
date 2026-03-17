#!/usr/bin/env python3
"""
Interactive WebSocket client for EmbedClaw.
Usage:
  pip install websocket-client
  python scripts/test_ws_client.py 192.168.31.32 18789
  python scripts/test_ws_client.py 192.168.31.32 18789 --trace
"""

import argparse
import json
import os
import socket
import sys
import threading

try:
    import websocket
except ImportError:
    print("Install: pip install websocket-client")
    sys.exit(1)


def _clear_proxy_env(host: str) -> None:
    for key in (
        "http_proxy",
        "https_proxy",
        "all_proxy",
        "HTTP_PROXY",
        "HTTPS_PROXY",
        "ALL_PROXY",
    ):
        os.environ.pop(key, None)
    no_proxy_vals = [host, "127.0.0.1", "localhost"]
    os.environ["NO_PROXY"] = ",".join(no_proxy_vals)
    os.environ["no_proxy"] = ",".join(no_proxy_vals)


def _tcp_check(host: str, port: int, timeout: float) -> None:
    print(f"TCP check: {host}:{port} ...")
    with socket.create_connection((host, port), timeout=timeout):
        print("TCP check ok.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Interactive WS client for EmbedClaw")
    parser.add_argument("host", nargs="?", default="192.168.31.32")
    parser.add_argument("port", nargs="?", type=int, default=18789)
    parser.add_argument("--trace", action="store_true", help="Enable websocket-client wire trace")
    parser.add_argument("--allow-proxy", action="store_true", help="Do not clear HTTP(S)_PROXY env")
    parser.add_argument("--timeout", type=float, default=5.0, help="Connect timeout in seconds")
    parser.add_argument("--chat-id", default="", help="Optional chat_id field to send in JSON")
    parser.add_argument("--chat-type", default="", help="Optional chat_type field to send in JSON")
    parser.add_argument("--channel", default="", help="Optional channel field to send in JSON")
    args = parser.parse_args()

    target_channel = args.channel.strip().lower()
    if target_channel and target_channel != "ws" and (not args.chat_id or not args.chat_type):
        parser.error("non-ws --channel requires both --chat-id and --chat-type")

    if not args.allow_proxy:
        _clear_proxy_env(args.host)

    if args.trace:
        websocket.enableTrace(True)

    print(
        "Proxy env: "
        f"HTTP_PROXY={os.getenv('HTTP_PROXY')}, "
        f"HTTPS_PROXY={os.getenv('HTTPS_PROXY')}, "
        f"NO_PROXY={os.getenv('NO_PROXY')}"
    )

    try:
        _tcp_check(args.host, args.port, args.timeout)
    except OSError as e:
        print(f"TCP check failed: {e}")
        sys.exit(2)

    url = f"ws://{args.host}:{args.port}/"
    print(f"Connecting to {url} ...")

    try:
        ws = websocket.create_connection(url, timeout=args.timeout, enable_multithread=True)
    except TypeError:
        ws = websocket.create_connection(url, timeout=args.timeout)
    except Exception as e:
        print(f"Connect failed: {e}")
        sys.exit(3)

    # Keep recv() blocking indefinitely; avoid treating idle periods as errors.
    ws.settimeout(None)

    print("Connected. Type message and press Enter.")
    print("Commands: /quit, /exit, /help, /json {raw_json}")

    stop_event = threading.Event()

    def recv_loop() -> None:
        while not stop_event.is_set():
            try:
                msg = ws.recv()
            except websocket.WebSocketTimeoutException:
                continue
            except Exception as e:
                if not stop_event.is_set():
                    print(f"\n[recv error] {e}")
                break
            if msg is None:
                continue
            print(f"\n<- {msg}")
            try:
                obj = json.loads(msg)
                if obj.get("type") == "response":
                    print(f"[Reply] {obj.get('content', '')}")
            except json.JSONDecodeError:
                pass
            print("> ", end="", flush=True)

    recv_thread = threading.Thread(target=recv_loop, daemon=True)
    recv_thread.start()

    try:
        while True:
            line = input("> ").strip()
            if not line:
                continue
            if line in ("/quit", "/exit"):
                break
            if line == "/help":
                print("Input text will be packaged as JSON and sent:")
                print('{"type":"message","content":"..."}')
                print("For non-ws channel, pass --channel + --chat-id + --chat-type.")
                print("Commands: /quit, /exit, /help, /json {raw_json}")
                continue
            if line.startswith("/json "):
                raw = line[6:].strip()
                if not raw:
                    print("Empty JSON payload.")
                    continue
                try:
                    json.loads(raw)
                except json.JSONDecodeError as e:
                    print(f"Invalid JSON: {e}")
                    continue
                ws.send(raw)
                print(f"-> {raw}")
                continue

            payload = {
                "type": "message",
                "content": line,
            }
            if args.chat_id:
                payload["chat_id"] = args.chat_id
            if args.chat_type:
                payload["chat_type"] = args.chat_type
            if args.channel:
                payload["channel"] = args.channel

            wire = json.dumps(payload, ensure_ascii=False)
            ws.send(wire)
            print(f"-> {wire}")
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        stop_event.set()
        try:
            ws.close()
        except Exception:
            pass
        recv_thread.join(timeout=1.0)
        print("Connection closed.")


if __name__ == "__main__":
    main()
