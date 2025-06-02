import socket
import threading
import subprocess
import sys
import time
import os
import argparse

PORT_C = 9998         # Controller port for READY signal
PORT_W_DEFAULT = 9999 # Idle device listening port
PORT_W_TASK = 10000   # Worker port for task handling
BUFFER_SIZE = 4096
DISCOVERY_MSG = "DISCOVER"
INVITE_MSG = "BECOME_WORKER"
TASK_MSG = "TASK"
READY_MSG = "READY"
TIMEOUT = 15  # Timeout in seconds

worker_active = False  # Prevent multiple worker threads

# ============ Utility Functions ============

def create_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    return s

def send_message(ip, port, message):
    with create_socket() as s:
        try:
            s.connect((ip, port))
            s.sendall(message.encode())
        except ConnectionRefusedError:
            print(f"[ERROR] Connection to {ip}:{port} refused")
        except socket.error as e:
            print(f"[ERROR] Socket error when sending to {ip}:{port}: {e}")

def run_node_task(script_content):
    with open("temp_task.js", "w") as f:
        f.write(script_content)
    result = subprocess.run(["node", "temp_task.js"], capture_output=True, text=True)
    os.remove("temp_task.js")
    if result.returncode != 0:
        return f"Error: {result.stderr}"
    return result.stdout.strip()

# ============ Worker Logic ============

def start_worker(server_ip):
    global worker_active
    if worker_active:
        print("[WORKER] Already active, ignoring additional invites.")
        return

    worker_active = True
    print(f"[WORKER] Starting worker, listening on {PORT_W_TASK}...")
    s = create_socket()
    try:
        s.bind(('', PORT_W_TASK))
        s.listen(1)
        s.settimeout(TIMEOUT)

        # Notify controller
        send_message(server_ip, PORT_C, READY_MSG)

        try:
            conn, addr = s.accept()
            print(f"[WORKER] Connected to controller at {addr}")
            conn.settimeout(TIMEOUT)

            while True:
                try:
                    data = conn.recv(BUFFER_SIZE).decode()
                    if not data:
                        print("[WORKER] Controller closed connection, reverting to idle...")
                        break
                    if data.startswith(TASK_MSG):
                        task = data[len(TASK_MSG):]
                        print(f"[WORKER] Received task, executing...")
                        output = run_node_task(task)
                        conn.sendall(f"RESULT:{output}".encode())
                        print(f"[WORKER] Result sent")
                        break
                    else:
                        print(f"[WORKER] Unknown message: {data}")
                except socket.timeout:
                    print("[WORKER] No task received, reverting to idle...")
                    break
                except socket.error as e:
                    print(f"[WORKER] Socket error: {e}")
                    break
        except socket.timeout:
            print("[WORKER] No connection received, reverting to idle...")
        except socket.error as e:
            print(f"[WORKER] Socket error: {e}")
    finally:
        worker_active = False
        s.close()

# ============ Controller Logic ============

def discover_devices(idle_ports):
    subnet = "127.0.0."  # Adjust for LAN
    print("[CTRL] Scanning for idle devices...")
    for i in range(2, 5):  # Range for localhost testing
        ip = subnet + str(i)
        port = idle_ports.get(ip, PORT_W_DEFAULT)
        send_message(ip, port, DISCOVERY_MSG)

def send_invite(ip, port):
    print(f"[CTRL] Sending invite to {ip}:{port}")
    send_message(ip, port, INVITE_MSG)

def handle_discovery(idle_port):
    def listener():
        s = create_socket()
        try:
            s.bind(('', idle_port))
            s.listen(5)
            print(f"[IDLE] Listening for discovery/invite on port {idle_port}...")
            while True:
                conn, addr = s.accept()
                try:
                    data = conn.recv(BUFFER_SIZE).decode()
                    if data == DISCOVERY_MSG:
                        print(f"[IDLE] Discovery ping from {addr}")
                    elif data == INVITE_MSG:
                        print(f"[IDLE] Invite to become worker from {addr}")
                        conn.close()
                        s.close()
                        start_worker(addr[0])
                        return
                    else:
                        print(f"[IDLE] Unknown message: {data}")
                finally:
                    conn.close()
        except socket.error as e:
            print(f"[IDLE] Socket error: {e}")
        finally:
            s.close()
    threading.Thread(target=listener, daemon=True).start()

def wait_for_worker_ready():
    s = create_socket()
    try:
        s.bind(('', PORT_C))
        s.listen(1)
        s.settimeout(TIMEOUT)
        print(f"[CTRL] Waiting for worker READY on port {PORT_C}...")
        conn, addr = s.accept()
        data = conn.recv(BUFFER_SIZE).decode()
        if data == READY_MSG:
            print(f"[CTRL] Worker at {addr[0]} is ready")
            return addr[0]
        else:
            print(f"[CTRL] Unexpected message: {data}")
            return None
    except socket.timeout:
        print("[CTRL] No READY signal received")
        return None
    except socket.error as e:
        print(f"[CTRL] Socket error: {e}")
        return None
    finally:
        s.close()

def send_task(ip, script_path):
    with open(script_path, 'r') as f:
        script = f.read()
    msg = TASK_MSG + script
    print(f"[CTRL] Sending task to {ip}:{PORT_W_TASK}...")
    with create_socket() as s:
        try:
            s.connect((ip, PORT_W_TASK))
            s.sendall(msg.encode())
            s.settimeout(TIMEOUT)
            result = s.recv(BUFFER_SIZE).decode()
            if result.startswith("RESULT:"):
                print(f"[CTRL] Result from worker: {result[len('RESULT:'):]}")
            else:
                print("[CTRL] Invalid result")
        except socket.timeout:
            print("[CTRL] Timeout waiting for task result")
        except socket.error as e:
            print(f"[CTRL] Socket error: {e}")

# ============ Main ============

def main():
    parser = argparse.ArgumentParser(description="Distributed Task Sharing")
    parser.add_argument("--controller", action="store_true", help="Run as controller")
    parser.add_argument("--idle-port", type=int, default=PORT_W_DEFAULT, help="Idle device listening port")
    args = parser.parse_args()

    idle_ports = {
        "127.0.0.2": 9999,
        "127.0.0.3": 10001,
        "127.0.0.4": 10002
    }

    if args.controller:
        print("[CTRL] Starting as controller...")
        discover_devices(idle_ports)
        target_ip = input("Enter IP of idle device to invite as worker: ").strip()
        target_port = idle_ports.get(target_ip, PORT_W_DEFAULT)
        send_invite(target_ip, target_port)

        worker_ip = wait_for_worker_ready()
        if worker_ip:
            script_path = "task.js"
            send_task(worker_ip, script_path)
        else:
            print("[CTRL] Worker not ready, exiting")
    else:
        print("[IDLE] Starting in idle mode...")
        while True:
            handle_discovery(args.idle_port)
            print("[IDLE] Discovery handler restarted...")
            time.sleep(5)

if __name__ == "__main__":
    main()
