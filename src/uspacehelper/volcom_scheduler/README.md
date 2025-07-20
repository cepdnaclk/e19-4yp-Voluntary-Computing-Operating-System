# Voluntary Computing Task Scheduler

This document outlines the functionality of the `volcom_scheduler`, which is responsible for managing tasks, breaking them into chunks, and assigning them to available employees based on a selected scheduling policy.

## Core Concepts

-   **Task**: A high-level job to be completed, defined by an input file, priority, and deadline.
-   **Chunk**: A smaller, distributable piece of a larger task. The scheduler breaks down a task's input file into multiple chunks.
-   **Employee**: A volunteer node that receives and processes chunks.
-   **Scheduling Policy**: The algorithm used to decide which available employee is best suited to receive the next task chunk.

## How it Works

The scheduler's primary decision-making function is `select_best_employee`. This function iterates through all available employees and calculates a "score" for each one based on the currently active scheduling policy. The employee with the highest score is chosen to receive the next chunk.

The default policy is **Round Robin**.

## Implemented Scheduling Policies

The scheduler currently supports the following four policies:

### 1. Round Robin (`SCHEDULE_ROUND_ROBIN`)

-   **Goal**: To distribute tasks as evenly as possible with minimal overhead.
-   **Logic**: This is a simple form of load balancing. The score is calculated based on the number of tasks an employee is currently handling. An employee with fewer active tasks gets a higher score.
-   **Formula**: `score = 100.0 - employee.active_tasks`
-   **Best For**: General-purpose workloads where employee capabilities are unknown or assumed to be similar.

### 2. Load Balanced (`SCHEDULE_LOAD_BALANCED`)

-   **Goal**: To assign tasks to the least-utilized employees, considering their real-time CPU and memory usage.
-   **Logic**: This policy calculates a score based on an employee's available resources. It heavily favors employees with low CPU usage, low memory usage, and a small number of active tasks.
-   **Formula**: `score = (100.0 - employee.cpu_usage) + (100.0 - employee.memory_usage) - (employee.active_tasks * 10.0)`
-   **Best For**: Heterogeneous environments where employee machines have varying capabilities and workloads.

### 3. Priority Based (`SCHEDULE_PRIORITY_BASED`)

-   **Goal**: To ensure that high-priority tasks are assigned to the most capable employees.
-   **Logic**: This policy combines the task's own priority with the employee's current CPU load. A high-priority task assigned to an idle employee will generate the highest score.
-   **Formula**: `score = (100.0 - employee.cpu_usage) * (task.priority / 10.0)`
-   **Best For**: Workloads where some tasks are more critical than others.

### 4. Deadline Aware (`SCHEDULE_DEADLINE_AWARE`)

-   **Goal**: To prioritize tasks that are close to their deadline.
-   **Logic**: This policy calculates the time remaining until a task's deadline and favors assigning it to employees with the fewest active tasks, increasing the chance of timely completion.
-   **Formula**: `score = time_remaining_for_task / (employee.active_tasks + 1)`
-   **Best For**: Time-sensitive computations where meeting deadlines is critical.

## How to Change the Policy

The scheduling policy can be changed at runtime by calling the `set_scheduling_policy()` function with one of the defined policy constants.