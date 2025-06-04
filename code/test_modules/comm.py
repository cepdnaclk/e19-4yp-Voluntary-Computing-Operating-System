import socket
import time
import json
import threading

BROADCAST_PORT = 50000
TCP_PORT = 50010

BROADCAST_INTERVAL = 5 

DEVICE_ID = input("Enter device ID (e.g., DeviceA): ") #Obtain from system
ROLE = "Idle" #Chage dynamically
known_devices = set()

def tcp_server():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(('', TCP_PORT))
    server_sock.listen()

    print(f"[{DEVICE_ID}] TCP server listening on port {TCP_PORT}...")

    while True:
        conn, addr = server_sock.accept()
        data = conn.recv(1024).decode()

        try:
            message = json.loads(data)

            if message.get("type") == "task":
                task_id = message.get("task_id")
                value = message.get("value")
                op = message.get("operation")

                print(f"[{DEVICE_ID}] Received task {task_id}: {op}({value})")

                if op == "square":
                    result = value * value
                else:
                    result = "Unsupported operation"

                conn.send(f"result: {result}".encode())
            else:
                conn.send("Unknown message type.".encode())

        except Exception as e:
            conn.send(f"Error processing task: {e}".encode())

        conn.close()

def initiate_tcp_handshake(peer_ip):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3)
        sock.connect((peer_ip, TCP_PORT))

        # Simulated task: compute square of 4
        task = {
            "type": "task",
            "task_id": f"task-{int(time.time())}",
            "operation": "square",
            "value": 4
        }

        sock.send(json.dumps(task).encode())

        response = sock.recv(1024).decode()
        print(f"[{DEVICE_ID}] Received task result: {response}")
        sock.close()

    except Exception as e:
        print(f"[{DEVICE_ID}] TCP task send failed with {peer_ip}: {e}")

def broadcast_presence():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    # No need to bind a port for sending only
    while True:
        message = {
            "type": "broadcast",
            "device_id": DEVICE_ID,
            "role": ROLE
        }
        try:
            sock.sendto(json.dumps(message).encode(), ('<broadcast>', BROADCAST_PORT))
            print(f"[{DEVICE_ID}] Broadcasted presence...")
        except Exception as e:
            print(f"[{DEVICE_ID}] Broadcast error: {e}")
        time.sleep(BROADCAST_INTERVAL)

def listen_for_broadcasts():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', BROADCAST_PORT))  # Use a fixed port for receiving

    print(f"[{DEVICE_ID}] Listening for broadcasts...")

    while True:
        data, addr = sock.recvfrom(1024)
        try:
            message = json.loads(data.decode())
            if message["type"] == "broadcast":
                peer_id = message["device_id"]
                peer_ip = addr[0]
                if peer_id != DEVICE_ID and peer_id not in known_devices:
                    known_devices.add(peer_id)
                    print(f"[{DEVICE_ID}] Discovered new device: {peer_id} ({message['role']}) at {peer_ip}")
                    threading.Thread(target=initiate_tcp_handshake, args=(peer_ip,), daemon=True).start()

        except Exception as e:
            print(f"[{DEVICE_ID}] Error decoding message: {e}")

if __name__ == "__main__":
    broadcast_thread = threading.Thread(target=broadcast_presence, daemon=True)
    listen_thread = threading.Thread(target=listen_for_broadcasts, daemon=True)
    tcp_server_thread = threading.Thread(target=tcp_server, daemon=True)

    tcp_server_thread.start()
    broadcast_thread.start()
    listen_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\n[{DEVICE_ID}] Shutting down.")
