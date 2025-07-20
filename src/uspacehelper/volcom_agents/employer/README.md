# Volcom Agent - Employer Mode

## Overview

The Employer is the central component of the Voluntary Computing System, responsible for discovering available volunteer nodes (Employees), distributing tasks, and collecting the results. It acts as the orchestrator, breaking down a large computational job into smaller chunks and managing the entire lifecycle of the distributed computation.

## How it Works

The employer operates in a **continuous, dynamic loop**, simultaneously handling employee discovery, task distribution, and result collection. This allows it to adapt to employees joining and leaving the network at any time.

### 1. Continuous Discovery and Task Management

The employer's main loop is an integrated process that handles all operations concurrently:

-   **Dynamic Employee Discovery**: The employer continuously listens for UDP broadcasts on port `9876`. New employees can be discovered and added to the resource pool at any point during the employer's runtime.
-   **Live Employee Roster**: It maintains a live list of available employees, adding new ones as they appear and pruning those that go silent using a `STALE_THRESHOLD`.
-   **Dynamic Task Distribution**: As soon as tasks are available and there are free employees, the employer assigns them using a round-robin strategy. This happens in parallel with the discovery process.
-   **TCP-based File Transfer**: For each task, the employer establishes a direct TCP connection to the selected employee on port `12345`. It sends the task metadata (task ID, filename) as a JSON object, followed by the actual file content.

### 2. Result Collection and Fault Tolerance

-   **Asynchronous Checking**: The employer periodically checks for the completion of tasks. In the current implementation, this is simulated by checking if a certain amount of time has passed since the task was assigned.
-   **Timeout and Reassignment**: If a task is not completed within a `TASK_TIMEOUT_SECONDS` (60 seconds), it is considered to have failed. The employer will mark the task for reassignment to another employee and may penalize the original employee's reliability score.

## Technologies Used

- **Programming Language**: C (C99 Standard)
- **Concurrency**: POSIX Threads (pthreads) for running the main discovery and task management loop in the background.
- **Networking**:
    - **UDP Sockets**: For connectionless, broadcast-based employee discovery.
    - **TCP Sockets**: For reliable, connection-oriented task file transfer.
    - **`select()`**: For non-blocking I/O to handle network events efficiently.
- **Data Interchange**: **cJSON** library for creating and parsing JSON-formatted messages for communication between the employer and employees.

## Example: Distributed Image Rendering

Let's imagine the employer needs to render 5 frames of a complex 3D animation. Each frame is a separate task.

1.  **Task Preparation**: The 5 scene files (`frame1.pov`, `frame2.pov`, ..., `frame5.pov`) are placed in the `/home/geeth99/Desktop/chuncked_set` directory.

2.  **Continuous Operation**: The employer starts its main loop.
    - It immediately begins listening for employee broadcasts.
    - `employee-A` comes online and is discovered. The employer assigns it `frame1.pov`.
    - `employee-B` comes online a few seconds later and is also discovered. The employer assigns it `frame2.pov`.
    - The employer continues its round-robin assignment: `frame3.pov` to `employee-A`, `frame4.pov` to `employee-B`, and `frame5.pov` to `employee-A`.

3.  **Dynamic Changes & Result Collection**:
    - While tasks are running, `employee-C` joins the network. The employer discovers it and adds it to the pool, ready for future tasks.
    - The employer waits for the results. As each employee finishes rendering its frame, it would (in a full implementation) send the resulting image (`frame1.png`, `frame2.png`, etc.) back to the employer.
    - If `employee-B` crashed while rendering `frame4.pov`, the task would time out. The employer would then reassign `frame4.pov` to the next available employee, which could be `employee-A` or the newly discovered `employee-C`.
