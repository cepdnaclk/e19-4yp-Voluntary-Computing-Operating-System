# VolCom Agents: Task Distribution and Communication

This document describes the architecture and operation of the VolCom agent system, which enables distributed task execution between an employer (controller) and multiple employees (workers) over a network. The system is designed for voluntary computing environments, where nodes can dynamically join or leave the network.

## Overview

The VolCom agent system consists of two main agent types:
- **Employer Agent**: Responsible for discovering employees, distributing tasks, and collecting results.
- **Employee Agent**: Listens for tasks, processes them, and returns results to the employer.

A hybrid mode is also possible, but the main focus is on the employer and employee roles.

---

## 1. Task Distribution Workflow

### Employer Agent
1. **Discovery**: Listens for UDP broadcast messages from employees advertising their availability and resource status (CPU, memory, etc.).
2. **Connection**: Establishes a persistent TCP connection to each discovered employee.
3. **Configuration**: Sends an initial configuration script to new employees to prepare them for task execution.
4. **Task Assignment**:
    - Scans a directory (e.g., `/home/geeth99/Desktop/chuncked_set`) for task files (e.g., `.json` chunks).
    - Assigns tasks to available employees using a round-robin or load-based strategy.
    - Sends task files and metadata over the persistent TCP connection.
5. **Result Collection**:
    - Listens for results from employees on the same TCP connection.
    - Saves results to a results directory (e.g., `/home/geeth99/Desktop/results`).
6. **Monitoring**:
    - Tracks employee status, task progress, and handles timeouts or failures.
    - Removes stale or disconnected employees.

### Employee Agent
1. **Broadcasting**: Periodically sends UDP broadcast messages with its status (free memory, CPU usage, etc.) to the network.
2. **Task Reception**: Listens for incoming tasks from the employer over TCP.
3. **Task Processing**:
    - Receives task files and metadata.
    - Processes the task (e.g., runs a script or computation).
    - Generates a result file.
4. **Result Sending**: Sends the result file and metadata back to the employer over the same TCP connection.
5. **Resource Awareness**: Pauses broadcasting if system resources are low.

---

## 2. Communication Methods

- **Discovery**: UDP broadcast (port 9876) for employee advertisement.
- **Task and Result Transfer**: Persistent TCP connections (port 12345) for reliable file and metadata transfer.
- **Data Format**: JSON is used for metadata (task info, status, etc.), while files are sent as binary streams.

---

## 3. Data Structures

- **Task Buffer**: Circular buffer for incoming tasks (employee side).
- **Result Queue**: Circular queue for outgoing results (employee side).
- **Task Assignment Table**: Tracks which tasks are assigned to which employees (employer side).
- **Employee List**: Tracks discovered employees and their status (employer side).

---

## 4. Fault Tolerance & Reliability

- **Timeouts**: Tasks are reassigned if not completed within a timeout period.
- **Retries**: Failed task transfers are retried, and employee reliability is adjusted.
- **Stale Removal**: Employees that stop broadcasting are removed from the active list.

---

## 5. Extensibility

- The system is modular, with clear separation between networking, task management, and agent logic.
- New task types or result handling strategies can be added by extending the relevant modules.

---

## 6. Directory Structure

- `employer_mode.c`: Employer agent logic (discovery, assignment, result collection).
- `employee_mode.c`: Employee agent logic (broadcast, task processing, result sending).
- `task_management.c`, `task_buffer.c`, `result_queue.c`: Task/result queue implementations.
- `volcom_agents.h`: Shared data structures and function prototypes.

---

## 7. How to Run

1. **Start Employer**: Run the employer agent on the controller node. It will listen for employees and distribute tasks found in the chunked set directory.
2. **Start Employees**: Run the employee agent on worker nodes. They will broadcast their presence and wait for tasks.
3. **Monitor**: The employer will print status updates, and results will be saved in the results directory.

---

## 8. Example Communication Flow

1. Employee broadcasts: `{ "type": "volcom_broadcast", ... }`
2. Employer receives broadcast, connects, and sends config.
3. Employer assigns a task: sends metadata + file.
4. Employee processes task, sends result metadata + file.
5. Employer receives and saves result.

---

## 9. References
- See code comments in each file for further details on implementation and extension.
