import subprocess
import time
import socket
import pytest
import urllib3
import os

SERVER_BIN = "./public/webserver"
PORT = 8888
BASE_URL = f"http://localhost:{PORT}"
TIMEOUT = 3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

def is_server_running(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', port)) == 0

@pytest.fixture(scope="module")
def running_server():
    proc = subprocess.Popen([SERVER_BIN], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    for _ in range(TIMEOUT * 10):
        if is_server_running(PORT):
            break
        time.sleep(0.1)
    else:
        proc.terminate()
        proc.wait()
        pytest.fail(f"Server failed to start on port {PORT}")

    yield proc

    proc.send_signal(subprocess.signal.SIGINT)
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()

# --- Тесты ---

def test_server_starts(running_server):
    assert is_server_running(PORT)

def test_serves_index_html(running_server):
    if not os.path.exists("index.html"):
        with open("index.html", "w") as f:
            f.write("<html><body><h1>Hello from WebServer</h1></body></html>")

    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/")
    assert resp.status == 200
    assert b"<html" in resp.data.lower()
    assert resp.headers.get("Content-Type") == "text/html"

def test_serves_existing_file(running_server):
    test_file = "public/test.txt"
    os.makedirs("public", exist_ok=True)
    with open(test_file, "w") as f:
        f.write("Hello from e2e test!")

    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/public/test.txt")
    assert resp.status == 200
    assert resp.data == b"Hello from e2e test!"
    assert resp.headers.get("Content-Type") == "application/octet-stream"

def test_serves_css(running_server):
    test_file = "public/style.css"
    with open(test_file, "w") as f:
        f.write("body { color: red; }")

    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/public/style.css")
    assert resp.status == 200
    assert resp.headers.get("Content-Type") == "text/css"

def test_serves_png(running_server):
    test_file = "public/test.png"
    png_header = bytes.fromhex("89504e470d0a1a0a")  # PNG signature
    with open(test_file, "wb") as f:
        f.write(png_header + b"\x00" * 100)

    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/public/test.png")
    assert resp.status == 200
    assert resp.headers.get("Content-Type") == "image/png"
    assert resp.data.startswith(png_header)

def test_404_not_found(running_server):
    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/nonexistent.file")
    assert resp.status == 404
    assert b"File Not Found" in resp.data
    assert resp.headers.get("Content-Type") == "text/plain"

def test_bad_request(running_server):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect(("localhost", PORT))
        s.send(b"NOT A HTTP REQUEST\r\n\r\n")
        response = s.recv(1024)
        assert b"400 Bad Request" in response or b"404" in response

def test_directory_traversal_blocked(running_server):
    http = urllib3.PoolManager()
    resp = http.request("GET", f"{BASE_URL}/../../../etc/passwd")
    assert resp.status in (400, 404)