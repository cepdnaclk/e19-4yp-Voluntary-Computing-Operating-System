# Volcom Agent - Employee Mode

## Overview

The Employee is the volunteer component of the Voluntary Computing System. It runs on a user's machine, and when active, it makes its computational resources available to an Employer node on the same network. It is responsible for broadcasting its availability, receiving tasks, processing them, and sending back the results.

## How it Works

The employee's operation is managed by several concurrent threads, allowing it to perform multiple actions simultaneously.

### 1. Resource Broadcasting

- **UDP Broadcasts**: The employee periodically (every 5 seconds) sends out a UDP broadcast message to the network on port `9876`.
- **JSON Status Message**: This broadcast is a JSON-formatted string containing:
    - Its unique ID (`employee_status.agent_id`).
    - Its IP address.
    - Current resource usage (CPU and memory percentage).
    - System specifications (CPU model, core count).
- **Resource Threshold**: Broadcasting is automatically paused if the machine's memory usage exceeds a `RESOURCE_THRESHOLD_PERCENT` (80%) to ensure the host system remains responsive for the user.

### 2. Task Reception

- **TCP Server**: The employee starts a TCP server that listens for incoming connections from employers on port `12345`.
- **Connection Handling**: When an employer connects, the employee first checks its current resource usage.
    - If resources are available, it sends an "ACCEPT" message.
    - If resources are high, it sends a "REJECT:HIGH_RESOURCE_USAGE" message.
- **File Reception**: After accepting a connection, it receives the task metadata (as a JSON object) and then the task file itself. The file is saved locally in the `/tmp/` directory.

### 3. Task Processing

- **Task Buffer**: Received tasks are placed into a thread-safe task buffer (a queue). This allows the employee to accept new tasks while still working on a current one.
- **Worker Thread**: A dedicated worker thread continuously checks the task buffer for new tasks.
- **Execution**: When a task is retrieved from the buffer, the worker thread simulates processing it. In a real-world scenario, this is where the actual computation (e.g., running a rendering command, executing a scientific calculation) would happen.
- **Result Generation**: After processing, a result file is generated (e.g., a rendered image, a data file).

### 4. Result Sending

- **Result Transmission (Future)**: After a task is successfully processed, the employee is responsible for connecting back to the employer and transmitting the result file. This part is a placeholder in the current implementation but would involve a TCP connection to the employer to send the results.

## Technologies Used

- **Programming Language**: C (C99 Standard)
- **Concurrency**: POSIX Threads (pthreads) are used extensively to manage:
    - The resource broadcasting loop.
    - The worker thread for task processing.
    - The main thread which listens for incoming employer connections.
- **Networking**:
    - **UDP Sockets**: For broadcasting availability to the network.
    - **TCP Sockets**: For creating a server to listen for and receive tasks from employers.
- **Data Interchange**: **cJSON** library for parsing task metadata received from the employer and for creating the resource broadcast messages.
- **System Information**: Utilizes the `volcom_sysinfo` module to get real-time CPU and memory usage.

## Example: Distributed Image Rendering

Continuing the example from the employer, an employee node would perform the following actions when assigned `frame1.pov`:

1.  **Accept Task**: The employee receives a connection request from the employer. Since its resource usage is low, it sends back an "ACCEPT" message.

2.  **Receive Task**: It receives the JSON metadata for `frame1.pov` and then the `.pov` file itself, saving it to `/tmp/employee_task_frame1.pov`.

3.  **Queue Task**: The task is added to the internal task buffer.

4.  **Process Task**: The worker thread picks up `frame1.pov` from the buffer. It would then execute a command like `povray /tmp/employee_task_frame1.pov -o /tmp/result_frame1.png`. This is simulated with a `sleep()` in the current code.

5.  **Send Result**: After the `povray` command completes and `/tmp/result_frame1.png` is created, the employee would connect back to the employer, send the result metadata, and then transmit the `frame1.png` file.
