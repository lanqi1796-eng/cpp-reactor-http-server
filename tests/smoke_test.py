#!/usr/bin/env python3
"""Minimal integration tests for the user's local Reactor HTTP server.

Run with the server already listening on 127.0.0.1:9999.
No external Python dependencies required.
"""
import http.client
import socket

HOST = "127.0.0.1"
PORT = 9999


def get_response(conn, method, path, body=None):
    conn.request(method, path, body=body)
    response = conn.getresponse()
    data = response.read()
    return response.status, dict((k.lower(), v) for k, v in response.getheaders()), data


def check_basic():
    conn = http.client.HTTPConnection(HOST, PORT, timeout=5)
    try:
        status, headers, body = get_response(conn, "GET", "/")
        assert status == 200, f"GET / status={status}"
        assert b"<html" in body.lower(), "GET / body not HTML"
        assert int(headers.get("content-length", "-1")) == len(body)
        print("[PASS] GET / HTML and Content-Length")

        status, headers, body = get_response(conn, "POST", "/echo", b"hello=world")
        assert status == 200, f"POST /echo status={status}"
        assert body == b"hello=world", f"POST echo wrong body: {body!r}"
        print("[PASS] POST /echo body integrity")

        status, _, _ = get_response(conn, "GET", "/does-not-exist-1234.html")
        assert status == 404, f"404 status={status}"
        print("[PASS] 404 response")
    finally:
        conn.close()


def check_pipeline():
    raw = (
        b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\nabc"
        b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )
    with socket.create_connection((HOST, PORT), timeout=5) as s:
        s.settimeout(5)
        s.sendall(raw)
        result = bytearray()
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            result.extend(chunk)
    data = bytes(result)
    assert data.startswith(b"HTTP/1.1 200"), "pipeline first status missing"
    sep = data.index(b"\r\n\r\n") + 4
    assert data[sep:sep + 3] == b"abc", "first response body incorrect"
    assert data[sep + 3:].startswith(b"HTTP/1.1 200"), "second response out of order"
    print("[PASS] POST+GET pipelined response order")


if __name__ == "__main__":
    check_basic()
    check_pipeline()
    print("Smoke test suite: PASS")
