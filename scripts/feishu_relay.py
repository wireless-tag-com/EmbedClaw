#!/usr/bin/env python3

import argparse
import json
import logging
import sys

try:
    import websocket
except ImportError:
    print("Install: pip install websocket-client", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Forward message to device WebSocket
# ---------------------------------------------------------------------------

def send_to_device(ws_url: str, chat_id: str, content: str) -> bool:
    """Send one message to EmbedClaw's WebSocket (JSON: type=message, channel=feishu)."""
    try:
        ws = websocket.create_connection(ws_url, timeout=5)
        payload = {
            "type": "message",
            "content": content,
            "chat_id": chat_id,
            "channel": "feishu",
        }
        ws.send(json.dumps(payload, ensure_ascii=False))
        ws.close()
        return True
    except Exception as e:
        logging.error("Send to device %s failed: %s", ws_url, e)
        return False


# ---------------------------------------------------------------------------
# Feishu event handling (long connection via lark-oapi)
# ---------------------------------------------------------------------------

def run_with_lark_oapi(app_id: str, app_secret: str, device_ws_url: str) -> None:
    try:
        from lark_oapi import Client, EventDispatcher
        from lark_oapi.api.event import v1 as event_v1
    except ImportError:
        logging.error("Install: pip install lark-oapi")
        sys.exit(1)

    # Build reply target: p2p -> open_id:xxx, group -> chat_id:xxx
    def build_chat_id(event: dict) -> str:
        event_data = event.get("event", {})
        sender = event_data.get("sender", {})
        sender_id = sender.get("sender_id", {})
        open_id = sender_id.get("open_id") or ""
        message = event_data.get("message", {})
        chat_type = message.get("chat_type") or "p2p"
        chat_id = message.get("chat_id") or ""
        if chat_type == "p2p" and open_id:
            return f"open_id:{open_id}"
        if chat_id:
            return f"chat_id:{chat_id}"
        return f"open_id:{open_id}" if open_id else ""

    def extract_text(event: dict) -> str:
        event_data = event.get("event", {})
        message = event_data.get("message", {})
        content_str = message.get("content") or "{}"
        try:
            content = json.loads(content_str)
            return content.get("text", content_str)
        except Exception:
            return content_str

    def handler(data: dict) -> None:
        event_type = (data.get("header") or {}).get("event_type")
        if event_type != "im.message.receive_v1":
            return
        chat_id = build_chat_id(data)
        text = extract_text(data)
        if not chat_id or not text:
            return
        logging.info("Feishu message -> device: chat_id=%s len=%d", chat_id, len(text))
        send_to_device(device_ws_url, chat_id, text)

    # Client with long connection (outbound WebSocket to Feishu)
    # See: https://open.feishu.cn/document/server-docs/event-subscription-guide/event-subscription-configure-/subscribe-to-events-via-websocket
    client = Client.builder() \
        .app_id(app_id) \
        .app_secret(app_secret) \
        .build()

    # EventDispatcher: register handler for im.message.receive_v1
    dispatcher = EventDispatcher()
    dispatcher.register("im.message.receive_v1", handler)

    logging.info("Starting Feishu long-connection relay -> %s", device_ws_url)
    logging.info("In Feishu console use '使用长连接接收事件', subscribe 接收消息 v2.0")

    # Start outbound WebSocket (SDK connects to Feishu; no public URL needed).
    # API may vary by lark-oapi version; see https://open.feishu.cn/document/server-docs/event-subscription-guide/event-subscription-configure-/subscribe-to-events-via-websocket
    outbound = getattr(client.event, "outbound", None) or getattr(client.event, "dispatcher", None)
    if outbound is not None and hasattr(outbound, "start"):
        outbound.start(dispatcher)
    elif hasattr(client, "event") and hasattr(client.event, "start_with_outbound"):
        client.event.start_with_outbound(dispatcher)
    else:
        logging.error(
            "lark-oapi outbound/long-connection API not found. "
            "Use Feishu console: 事件订阅 -> 使用长连接接收事件; subscribe 接收消息 v2.0. "
            "Then run your own script that on im.message.receive_v1 sends to device WS: "
            '{"type":"message","content":"...","chat_id":"open_id:ou_xxx","channel":"feishu"}'
        )
        sys.exit(1)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")

    ap = argparse.ArgumentParser(description="Feishu long-connection relay for EmbedClaw (no public IP)")
    ap.add_argument("--app_id", required=True, help="Feishu app_id (e.g. cli_xxx)")
    ap.add_argument("--app_secret", required=True, help="Feishu app_secret")
    ap.add_argument(
        "--device_ws",
        default="ws://192.168.31.33:18789",
        help="Device WebSocket URL (default: ws://192.168.31.33:18789)",
    )
    args = ap.parse_args()

    run_with_lark_oapi(args.app_id, args.app_secret, args.device_ws.rstrip("/"))


if __name__ == "__main__":
    main()
