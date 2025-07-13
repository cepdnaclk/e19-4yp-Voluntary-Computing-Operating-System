Navigate to volcom_comm directory:

cd /path/to/e19-4yp-Voluntary-Computing-Operating-System/volcom_comm

Build the project:
make

Run the communication module:
./volcom_comm


Choose Employee Mode:

When prompted:
Select mode:
1. Employer
2. Employee
Enter choice:
Enter: 2 and press Enter

Observe the Output:

You’ll see system resource JSON being broadcasted every 5 seconds, like:

[Broadcasting] {
  "ip":"192.168.1.42",
  "free_mem_mb":2048,
  "mem_usage_percent":24.57,
  "cpu_usage_percent":9.32,
  "cpu_model":"Intel(R) Core i5-8265U",
  "logical_cores":8
}

Automatic Pause:
If memory usage exceeds the threshold, broadcasting pauses:
[Broadcasting Paused] High memory usage: 93.4%
Stop Execution:
Press Ctrl+C to stop the employee node.
