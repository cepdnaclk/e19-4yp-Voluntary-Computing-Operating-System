import socket
import json
import subprocess
import os
import uuid
import argparse
import time
import base64

TCP_PORT = 5005
DEVICE_ID = "VoluntaryNode"

def execute_node_script(script_code):
    task_id = str(uuid.uuid4())
    filename = f"task_{task_id}.js"

    with open(filename, "w") as f:
        f.write(f"""
            (async () => {{
                try {{
                    const result = await ({script_code})();
                    console.log(result);
                }} catch (err) {{
                    console.error("Execution error:", err.message);
                }}
            }})();
        """)

    try:
        output = subprocess.check_output(["node", filename], stderr=subprocess.STDOUT)
        result = output.decode().strip()
    except subprocess.CalledProcessError as e:
        result = f"[ERROR] Node.js: {e.output.decode().strip()}"
    finally:
        os.remove(filename)

    return result


def run_worker():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(('', TCP_PORT))
    server_sock.listen()

    print(f"[{DEVICE_ID}] Worker listening on port {TCP_PORT}...")

    while True:
        conn, addr = server_sock.accept()
        data = conn.recv(8192).decode()
        try:
            message = json.loads(data)
            if message.get("type") == "task" and message.get("engine") == "node":
                print(f"[{DEVICE_ID}] Executing Node.js task...")
                script = message.get("script")
                result = execute_node_script(script)
                conn.send(result.encode())
            else:
                conn.send("Unsupported task type".encode())
        except Exception as e:
            conn.send(f"Error: {e}".encode())
        conn.close()


def run_controller(target_ip):
    time.sleep(1)  # Short delay to ensure worker is ready
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((target_ip, TCP_PORT))

        # Example heavy load task
        task = {
            "type": "task",
            "engine": "node",
            "task_id": "heavy-load-001",
            "script": """
                async () => {
                    const { createCanvas } = require('canvas');
                    const width = 300, height = 300;
                    const canvas = createCanvas(width, height);
                    const ctx = canvas.getContext('2d');

                    for (let x = 0; x < width; x++) {
                        for (let y = 0; y < height; y++) {
                            let m = mandelbrot((x - width/2)/100, (y - height/2)/100);
                            let color = Math.floor(m * 255);
                            ctx.fillStyle = `rgb(${color}, ${color}, ${color})`;
                            ctx.fillRect(x, y, 1, 1);
                        }
                    }

                    function mandelbrot(cx, cy) {
                        let x = 0, y = 0, iter = 0, max = 100;
                        while (x*x + y*y < 4 && iter < max) {
                            let xTemp = x*x - y*y + cx;
                            y = 2*x*y + cy;
                            x = xTemp;
                            iter++;
                        }
                        return iter / max;
                    }

                    return canvas.toDataURL(); // Return base64-encoded PNG
                }

            """
        }

        sock.send(json.dumps(task).encode())

        result = sock.recv(8192).decode()
        print(f"[Controller] Received result:\n{result}")
                

        if result.startswith("data:image/png;base64,"):
            img_data = result.split(",")[1]
            with open("rendered.png", "wb") as f:
                f.write(base64.b64decode(img_data))
            print("[Controller] Image saved as rendered.png")
        else:
            print("[Controller] Result:\n", result)

        sock.close()

    except Exception as e:
        print(f"[Controller] Error: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--role", choices=["controller", "worker"], required=True, help="Role of this instance")
    parser.add_argument("--target", help="IP address of worker (required for controller)")
    args = parser.parse_args()

    if args.role == "worker":
        run_worker()
    elif args.role == "controller":
        if not args.target:
            print("Error: --target is required for controller role")
        else:
            run_controller(args.target)
