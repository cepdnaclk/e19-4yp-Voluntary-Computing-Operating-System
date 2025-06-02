# import socket
# import threading
# import subprocess
# import sys
# import time
# import os
# import argparse

# PORT_C = 9998         # Controller port for READY signal
# PORT_W_DEFAULT = 9999 # Idle device listening port
# PORT_W_TASK = 10000   # Worker port for task handling
# BUFFER_SIZE = 4096
# DISCOVERY_MSG = "DISCOVER"
# INVITE_MSG = "BECOME_WORKER"
# TASK_MSG = "TASK"
# READY_MSG = "READY"
# TIMEOUT = 15  # Timeout in seconds

# worker_active = False  # Prevent multiple worker threads

# # ============ Utility Functions ============

# def create_socket():
#     s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#     return s

# def send_message(ip, port, message):
#     with create_socket() as s:
#         try:
#             s.connect((ip, port))
#             s.sendall(message.encode())
#         except ConnectionRefusedError:
#             print(f"[ERROR] Connection to {ip}:{port} refused")
#         except socket.error as e:
#             print(f"[ERROR] Socket error when sending to {ip}:{port}: {e}")

# def run_node_task(script_content):
#     with open("temp_task.js", "w") as f:
#         f.write(script_content)
#     result = subprocess.run(["node", "temp_task.js"], capture_output=True, text=True)
#     os.remove("temp_task.js")
#     if result.returncode != 0:
#         return f"Error: {result.stderr}"
#     return result.stdout.strip()

# # ============ Worker Logic ============

# def start_worker(server_ip):
#     global worker_active
#     if worker_active:
#         print("[WORKER] Already active, ignoring additional invites.")
#         return

#     worker_active = True
#     print(f"[WORKER] Starting worker, listening on {PORT_W_TASK}...")
#     s = create_socket()
#     try:
#         s.bind(('', PORT_W_TASK))
#         s.listen(1)
#         s.settimeout(TIMEOUT)

#         # Notify controller
#         send_message(server_ip, PORT_C, READY_MSG)

#         try:
#             conn, addr = s.accept()
#             print(f"[WORKER] Connected to controller at {addr}")
#             conn.settimeout(TIMEOUT)

#             while True:
#                 try:
#                     data = conn.recv(BUFFER_SIZE).decode()
#                     if not data:
#                         print("[WORKER] Controller closed connection, reverting to idle...")
#                         break
#                     if data.startswith(TASK_MSG):
#                         task = data[len(TASK_MSG):]
#                         print(f"[WORKER] Received task, executing...")
#                         output = run_node_task(task)
#                         conn.sendall(f"RESULT:{output}".encode())
#                         print(f"[WORKER] Result sent")
#                         break
#                     else:
#                         print(f"[WORKER] Unknown message: {data}")
#                 except socket.timeout:
#                     print("[WORKER] No task received, reverting to idle...")
#                     break
#                 except socket.error as e:
#                     print(f"[WORKER] Socket error: {e}")
#                     break
#         except socket.timeout:
#             print("[WORKER] No connection received, reverting to idle...")
#         except socket.error as e:
#             print(f"[WORKER] Socket error: {e}")
#     finally:
#         worker_active = False
#         s.close()

# # ============ Controller Logic ============

# def discover_devices(idle_ports):
#     subnet = "127.0.0."  # Adjust for LAN
#     print("[CTRL] Scanning for idle devices...")
#     for i in range(2, 5):  # Range for localhost testing
#         ip = subnet + str(i)
#         port = idle_ports.get(ip, PORT_W_DEFAULT)
#         send_message(ip, port, DISCOVERY_MSG)

# def send_invite(ip, port):
#     print(f"[CTRL] Sending invite to {ip}:{port}")
#     send_message(ip, port, INVITE_MSG)

# def handle_discovery(idle_port):
#     def listener():
#         s = create_socket()
#         try:
#             s.bind(('', idle_port))
#             s.listen(5)
#             print(f"[IDLE] Listening for discovery/invite on port {idle_port}...")
#             while True:
#                 conn, addr = s.accept()
#                 try:
#                     data = conn.recv(BUFFER_SIZE).decode()
#                     if data == DISCOVERY_MSG:
#                         print(f"[IDLE] Discovery ping from {addr}")
#                     elif data == INVITE_MSG:
#                         print(f"[IDLE] Invite to become worker from {addr}")
#                         conn.close()
#                         s.close()
#                         start_worker(addr[0])
#                         return
#                     else:
#                         print(f"[IDLE] Unknown message: {data}")
#                 finally:
#                     conn.close()
#         except socket.error as e:
#             print(f"[IDLE] Socket error: {e}")
#         finally:
#             s.close()
#     threading.Thread(target=listener, daemon=True).start()

# def wait_for_worker_ready():
#     s = create_socket()
#     try:
#         s.bind(('', PORT_C))
#         s.listen(1)
#         s.settimeout(TIMEOUT)
#         print(f"[CTRL] Waiting for worker READY on port {PORT_C}...")
#         conn, addr = s.accept()
#         data = conn.recv(BUFFER_SIZE).decode()
#         if data == READY_MSG:
#             print(f"[CTRL] Worker at {addr[0]} is ready")
#             return addr[0]
#         else:
#             print(f"[CTRL] Unexpected message: {data}")
#             return None
#     except socket.timeout:
#         print("[CTRL] No READY signal received")
#         return None
#     except socket.error as e:
#         print(f"[CTRL] Socket error: {e}")
#         return None
#     finally:
#         s.close()

# def send_task(ip, script_path):
#     with open(script_path, 'r') as f:
#         script = f.read()
#     msg = TASK_MSG + script
#     print(f"[CTRL] Sending task to {ip}:{PORT_W_TASK}...")
#     with create_socket() as s:
#         try:
#             s.connect((ip, PORT_W_TASK))
#             s.sendall(msg.encode())
#             s.settimeout(TIMEOUT)
#             result = s.recv(BUFFER_SIZE).decode()
#             if result.startswith("RESULT:"):
#                 print(f"[CTRL] Result from worker: {result[len('RESULT:'):]}")
#             else:
#                 print("[CTRL] Invalid result")
#         except socket.timeout:
#             print("[CTRL] Timeout waiting for task result")
#         except socket.error as e:
#             print(f"[CTRL] Socket error: {e}")

# # ============ Main ============

# def main():
#     parser = argparse.ArgumentParser(description="Distributed Task Sharing")
#     parser.add_argument("--controller", action="store_true", help="Run as controller")
#     parser.add_argument("--idle-port", type=int, default=PORT_W_DEFAULT, help="Idle device listening port")
#     args = parser.parse_args()

#     idle_ports = {
#         "127.0.0.2": 9999,
#         "127.0.0.3": 10001,
#         "127.0.0.4": 10002
#     }

#     if args.controller:
#         print("[CTRL] Starting as controller...")
#         discover_devices(idle_ports)
#         target_ip = input("Enter IP of idle device to invite as worker: ").strip()
#         target_port = idle_ports.get(target_ip, PORT_W_DEFAULT)
#         send_invite(target_ip, target_port)

#         worker_ip = wait_for_worker_ready()
#         if worker_ip:
#             script_path = "task.js"
#             send_task(worker_ip, script_path)
#         else:
#             print("[CTRL] Worker not ready, exiting")
#     else:
#         print("[IDLE] Starting in idle mode...")
#         while True:
#             handle_discovery(args.idle_port)
#             print("[IDLE] Discovery handler restarted...")
#             time.sleep(5)

# if __name__ == "__main__":
#     main()
import socket
import threading
import subprocess
import sys
import time
import os
import argparse
import json

# Constants
UDP_BROADCAST_PORT = 10001  # UDP port for broadcasting presence
CTRL_LISTEN_PORT = 10002    # TCP port for controller to send invites
WORKER_TASK_PORT = 10003    # TCP port for workers to receive tasks
BUFFER_SIZE = 4096          # Buffer size for socket communication
TIMEOUT = 10                # Worker inactivity timeout in seconds

# Message types
PRESENCE_MSG = "IDLE_PRESENCE"
INVITE_MSG = "BECOME_WORKER"
TASK_MSG = "TASK"
RESULT_MSG = "RESULT"

# Global flag to manage worker state
worker_active = False

# --- Utility Functions ---

def get_local_ip():
    """Retrieve the local IP address of the device."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

def create_udp_socket():
    """Create and configure a UDP socket for broadcasting."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    return s

def create_tcp_socket():
    """Create and configure a TCP socket with reuse address."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    return s

def send_tcp_message(ip, port, message):
    """Send a TCP message to the specified IP and port."""
    with create_tcp_socket() as s:
        try:
            s.connect((ip, port))
            s.sendall(message.encode())
        except socket.error as e:
            print(f"[ERROR] Failed to send to {ip}:{port} - {e}")

def run_node_task(script_content):
    """Execute a Node.js script and return the output."""
    with open("temp_task.js", "w") as f:
        f.write(script_content)
    result = subprocess.run(["node", "temp_task.js"], capture_output=True, text=True)
    os.remove("temp_task.js")
    return result.stdout.strip() if result.returncode == 0 else f"Error: {result.stderr}"

# --- Idle Device Functions ---

def broadcast_presence():
    """Periodically broadcast presence and system specs via UDP."""
    udp_sock = create_udp_socket()
    message = json.dumps({
        "type": PRESENCE_MSG,
        "ip": get_local_ip(),
        "specs": "CPU: 4 cores, RAM: 8GB"  # Static example specs
    })
    while True:
        # Send to '127.0.0.1' for localhost testing
        udp_sock.sendto(message.encode(), ('127.0.0.1', UDP_BROADCAST_PORT))
        print("[IDLE] Broadcasting presence...")
        time.sleep(5)

def listen_for_invite():
    """Listen for controller invitations to become a worker."""
    tcp_sock = create_tcp_socket()
    tcp_sock.bind(('', CTRL_LISTEN_PORT))
    tcp_sock.listen(1)
    print(f"[IDLE] Listening for invites on port {CTRL_LISTEN_PORT}...")
    while True:
        conn, addr = tcp_sock.accept()
        data = conn.recv(BUFFER_SIZE).decode()
        if data == INVITE_MSG:
            print(f"[IDLE] Invite received from {addr}")
            conn.close()
            tcp_sock.close()
            start_worker(addr[0])
            return
        conn.close()

def idle_device_mode():
    """Run the script in idle device mode."""
    threading.Thread(target=broadcast_presence, daemon=True).start()
    listen_for_invite()

# --- Worker Functions ---

def start_worker(controller_ip):
    """Start worker mode to receive and execute tasks."""
    global worker_active
    if worker_active:
        print("[WORKER] Already active, ignoring invite.")
        return
    worker_active = True
    print(f"[WORKER] Starting, listening on port {WORKER_TASK_PORT}...")
    tcp_sock = create_tcp_socket()
    try:
        tcp_sock.bind(('', WORKER_TASK_PORT))
        tcp_sock.listen(1)
        tcp_sock.settimeout(TIMEOUT)
        try:
            conn, addr = tcp_sock.accept()
            print(f"[WORKER] Connected to controller at {addr}")
            conn.settimeout(TIMEOUT)
            while True:
                try:
                    data = conn.recv(BUFFER_SIZE).decode()
                    if not data:
                        print("[WORKER] Connection closed by controller, reverting to idle...")
                        break
                    if data.startswith(TASK_MSG):
                        task = data[len(TASK_MSG):]
                        print("[WORKER] Executing task...")
                        output = run_node_task(task)
                        conn.sendall(f"{RESULT_MSG}{output}".encode())
                        print("[WORKER] Result sent to controller")
                    else:
                        print(f"[WORKER] Unknown message: {data}")
                except socket.timeout:
                    print("[WORKER] No task received within 10s, reverting to idle...")
                    break
        except socket.timeout:
            print("[WORKER] No connection within 10s, reverting to idle...")
    finally:
        worker_active = False
        tcp_sock.close()
        idle_device_mode()  # Revert to idle mode

# --- Controller Functions ---

def discover_idle_devices():
    """Discover idle devices broadcasting their presence."""
    udp_sock = create_udp_socket()
    udp_sock.bind(('', UDP_BROADCAST_PORT))
    print("[CTRL] Listening for idle devices...")
    idle_devices = {}
    start_time = time.time()
    while time.time() - start_time < 10:  # Listen for 10 seconds
        try:
            udp_sock.settimeout(1)  # Short timeout to allow periodic checks
            data, addr = udp_sock.recvfrom(BUFFER_SIZE)
            message = json.loads(data.decode())
            if message["type"] == PRESENCE_MSG:
                idle_devices[message["ip"]] = message["specs"]
                print(f"[CTRL] Found: {message['ip']} - {message['specs']}")
        except socket.timeout:
            continue
    return idle_devices

def controller_mode():
    """Run the script in controller mode."""
    idle_devices = discover_idle_devices()
    if not idle_devices:
        print("[CTRL] No idle devices found.")
        return
    print("\n[CTRL] Available idle devices:")
    for ip, specs in idle_devices.items():
        print(f"  {ip}: {specs}")
    target_ip = input("[CTRL] Enter IP of device to invite as worker: ").strip()
    if target_ip not in idle_devices:
        print("[CTRL] Invalid IP selected.")
        return
    send_tcp_message(target_ip, CTRL_LISTEN_PORT, INVITE_MSG)
    time.sleep(1)  # Give worker time to start
    with open("task.js", "r") as f:
        script = f.read()
    msg = TASK_MSG + script
    with create_tcp_socket() as s:
        try:
            s.connect((target_ip, WORKER_TASK_PORT))
            s.sendall(msg.encode())
            s.settimeout(TIMEOUT)
            result = s.recv(BUFFER_SIZE).decode()
            if result.startswith(RESULT_MSG):
                print(f"[CTRL] Worker result: {result[len(RESULT_MSG):]}")
            else:
                print("[CTRL] Invalid result received")
        except socket.error as e:
            print(f"[CTRL] Task sending failed: {e}")

# --- Main Entry Point ---

def main():
    """Parse arguments and start the script in the appropriate mode."""
    parser = argparse.ArgumentParser(description="Distributed Task Sharing System")
    parser.add_argument("--controller", action="store_true", help="Run as controller")
    args = parser.parse_args()
    if args.controller:
        print("[CTRL] Running as controller...")
        controller_mode()
    else:
        print("[IDLE] Running as idle device...")
        idle_device_mode()

if __name__ == "__main__":
    main()