
# VolCom Agent System: Technical Workflow

This document provides a detailed technical overview of how the VolCom system distributes, executes, and collects results for tasks between the employer (controller) and employee (worker) agents. It covers data flow, metadata management, synchronization, and storage mechanisms.

---

## 1. Task Discovery and Assignment (Employer Side)

### Task Source

- **Task Files Location:**  
  The employer scans a directory (e.g., `/home/geeth99/Desktop/chuncked_set`) for `.json` files. Each file represents a data chunk or task to be processed.

- **Script File:**  
  A configuration script (e.g., `script.py`) is also stored in the same directory and sent to new employees for initial setup.

### Task Metadata Creation

- **Metadata Structure:**  
  For each task, a `task_assignment_t` structure is created, containing:
  - `task_id`: Unique identifier (e.g., filename or generated string)
  - `chunk_file`: Path to the data chunk file
  - `employee_id`, `employee_ip`: Set when assigned
  - `assigned_time`, `is_completed`, `is_sent`, `retry_count`, etc.

- **Metadata Storage:**  
  All task assignments are stored in a static array:  
  `static task_assignment_t task_assignments[MAX_TASK_ASSIGNMENTS];`  
  The count is tracked by `assignment_count`.  
  This array acts as the central metadata table for all tasks.

- **Synchronization:**  
  Access to `task_assignments` is protected by `assignment_mutex` (a `pthread_mutex_t`) to ensure thread safety during concurrent reads/writes.

### Task Distribution

- **Assignment Strategy:**  
  Tasks are assigned to employees using a round-robin or load-based approach.  
  The function `distribute_new_tasks()` selects the next available employee (with less than 3 active tasks) and assigns a task.

- **Assignment Process:**  
  - The task's metadata is updated with the selected employee's info.
  - The assignment is added to the `task_assignments` array.
  - The employee's `active_tasks` count is incremented.

---

## 2. Task Sending and Execution (Employer → Employee)

### Persistent Connection

- **Connection Establishment:**  
  The employer maintains a persistent TCP connection to each employee (port 12345).

- **Sending Tasks:**  
  - The employer sends task metadata as a JSON object, followed by the binary data of the chunk file.
  - The function `send_file_to_employee()` handles this, sending metadata first, then the file size, then the file content in chunks.

- **Synchronization:**  
  Employee list access is protected by `employee_mutex` to avoid race conditions when updating employee state or connections.

---

## 3. Task Reception and Processing (Employee Side)

### Task Buffer

- **Reception:**  
  The employee listens for incoming tasks on the persistent TCP connection.

- **Buffering:**  
  Received tasks are stored in a circular buffer (`task_buffer_t task_buffer`), implemented in `task_management.c`.  
  - Functions: `add_task_to_buffer()`, `get_task_from_buffer()`
  - Protected by a mutex (`buffer->mutex`) to ensure thread-safe access.

- **Metadata Handling:**  
  Each received task is represented as a `received_task_t` structure, containing:
  - `task_id`, `chunk_filename`, `sender_id`, `received_time`, `is_processed`, `data`, `data_size`
  - The file is saved locally (e.g., `/tmp/employee_task_<task_id>`).

### Task Execution

- **Worker Thread:**  
  A dedicated worker thread (`worker_loop`) fetches tasks from the buffer, processes them (e.g., runs the script on the data), and generates a result file.

---

## 4. Result Handling and Sending (Employee → Employer)

### Result Queue

- **Result Preparation:**  
  After processing, results are stored in a circular result queue (`result_queue_t result_queue`), implemented in `task_management.c`.
  - Functions: `add_result_to_queue()`, `get_result_from_queue()`
  - Protected by a mutex (`queue->mutex`) for thread safety.

- **Result Metadata:**  
  Each result is a `result_info_t` structure, containing:
  - `task_id`, `result_filepath`, etc.

- **Sending Results:**  
  - The employee sends result metadata as JSON, followed by the result file size and content, over the same persistent TCP connection.
  - The function `send_result_to_employer()` handles this process.

---

## 5. Result Reception and Storage (Employer Side)

- **Reception:**  
  The employer listens for incoming results on each employee's TCP connection.

- **Storage:**  
  Results are saved to a results directory (e.g., `/home/geeth99/Desktop/results/result_<task_id>`).

- **Metadata Update:**  
  The corresponding `task_assignment_t` entry is updated (`is_completed = true`, `completed_time = ...`).  
  This update is protected by `assignment_mutex`.

---

## 6. Synchronization and Thread Safety

- **Employer Side:**
  - `assignment_mutex`: Protects the task assignment table.
  - `employee_mutex`: Protects the employee list and their state.
- **Employee Side:**
  - `task_buffer.mutex`: Protects the task buffer.
  - `result_queue.mutex`: Protects the result queue.

**Why Mutexes?**  
Multiple threads (e.g., main loop, worker, broadcaster) may access or modify shared data structures. Mutexes prevent race conditions, data corruption, and ensure consistent state.

---

## 7. Data Flow Summary

1. **Employer scans** the chunked set directory for `.json` files and creates task assignments.
2. **Employer discovers** employees via UDP broadcast, establishes TCP connections, and sends initial config scripts.
3. **Employer assigns** tasks to employees, sending metadata and data over TCP.
4. **Employee receives** tasks, buffers them, and processes them in a worker thread.
5. **Employee enqueues** results in the result queue after processing.
6. **Employee sends** results (metadata + file) back to the employer over TCP.
7. **Employer receives** results, saves them, and updates task metadata.

---

## 8. Key Storage Locations

- **Task files (input):** `/home/geeth99/Desktop/chuncked_set/*.json`
- **Script file:** `/home/geeth99/Desktop/chuncked_set/script.py`
- **Task metadata (employer):** In-memory array `task_assignments` (not persisted to disk)
- **Shared tasks (employee):** In-memory buffer `task_buffer`, files saved to `/tmp/employee_task_<task_id>`
- **Results (employee):** In-memory queue `result_queue`, files saved locally before sending
- **Results (employer):** `/home/geeth99/Desktop/results/result_<task_id>`

---

## 9. Metadata Creation and Handling

- **Employer:**  
  Creates and manages task metadata in `task_assignment_t` structures, updates status as tasks are assigned, sent, completed, or retried.

- **Employee:**  
  Receives metadata as JSON, stores it in `received_task_t` for processing, and in `result_info_t` for results.

- **All metadata is handled in-memory** and protected by mutexes for thread safety.

---

## 10. Summary Table

| Component         | Data Structure         | Storage Location                | Mutex/Synchronization      |
|-------------------|-----------------------|---------------------------------|----------------------------|
| Employer Tasks    | `task_assignment_t[]` | In-memory (static array)        | `assignment_mutex`         |
| Employer Employees| `employee_node_t*[]`  | In-memory (static array)        | `employee_mutex`           |
| Employee Tasks    | `task_buffer_t`       | In-memory (circular buffer)     | `task_buffer.mutex`        |
| Employee Results  | `result_queue_t`      | In-memory (circular queue)      | `result_queue.mutex`       |
| Files (chunks)    | N/A                   | `/home/geeth99/Desktop/chuncked_set` | N/A                  |
| Files (results)   | N/A                   | `/home/geeth99/Desktop/results` | N/A                        |

---

## 11. Threading Model

- **Employer:**  
  Main loop (discovery, assignment, result collection), plus possible worker threads for other tasks.

- **Employee:**  
  - Main thread (TCP server)
  - Broadcaster thread (UDP status)
  - Worker thread (task execution)

All shared data is protected by mutexes to ensure safe concurrent access.

---

## 12. Extensibility

- The system is modular: task management, networking, and agent logic are separated.
- New task types, result handling, or distribution strategies can be added by extending the relevant modules.

---

This technical overview should help developers understand the internal workflow, data flow, and synchronization mechanisms of the VolCom agent system. For further details, refer to code comments and the provided source files.
