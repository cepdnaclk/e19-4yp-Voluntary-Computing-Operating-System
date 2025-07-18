#include "volcom_uscheduler.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

// Test helper functions
void create_test_task(struct task_info_s *task, int id, const char *name, int priority) {
    task->task_id = id;
    strncpy(task->task_name, name, sizeof(task->task_name) - 1);
    task->task_name[sizeof(task->task_name) - 1] = '\0';
    task->priority = priority;
    task->memory_usage = 1024 * (id + 1); // Simulate memory usage
    task->cpu_usage = 10.0 + (id * 5.0);  // Simulate CPU usage
    strncpy(task->status, "pending", sizeof(task->status) - 1);
    
    // Initialize client info
    snprintf(task->client.ip_address, sizeof(task->client.ip_address), "192.168.1.%d", 100 + id);
    task->client.port = 8000 + id;
    snprintf(task->client.client_name, sizeof(task->client.client_name), "client_%d", id);
}

void print_task(const struct task_info_s *task) {
    printf("Task ID: %d, Name: %s, Priority: %d, Status: %s\n", 
           task->task_id, task->task_name, task->priority, task->status);
    printf("  Memory: %lu KB, CPU: %.1f%%, Client: %s (%s:%d)\n",
           task->memory_usage, task->cpu_usage, task->client.client_name,
           task->client.ip_address, task->client.port);
}

int main() {
    printf("=== Volcom User Scheduler Test ===\n\n");
    
    // Test 1: Initialize buffer
    printf("1. Testing buffer initialization...\n");
    struct task_buffer buffer;
    task_buffer_init(&buffer);
    
    assert(task_buffer_size(&buffer) == 0);
    printf("   ✓ Buffer initialized successfully\n");
    printf("   ✓ Initial size: %d\n", task_buffer_size(&buffer));
    
    // Test 2: Add tasks to buffer
    printf("\n2. Testing task enqueue...\n");
    struct task_info_s tasks[5];
    
    create_test_task(&tasks[0], 1, "DataProcessing", 1);
    create_test_task(&tasks[1], 2, "ImageAnalysis", 2);
    create_test_task(&tasks[2], 3, "NetworkSync", 3);
    create_test_task(&tasks[3], 4, "DatabaseQuery", 1);
    create_test_task(&tasks[4], 5, "FileCompression", 2);
    
    for (int i = 0; i < 5; i++) {
        bool success = task_buffer_enqueue(&buffer, &tasks[i]);
        assert(success);
        printf("   ✓ Added task %d: %s\n", tasks[i].task_id, tasks[i].task_name);
    }
    
    printf("   ✓ Buffer size after adding 5 tasks: %d\n", task_buffer_size(&buffer));
    
    // Test 3: Print buffer state
    printf("\n3. Current buffer state:\n");
    task_buffer_print(&buffer);
    
    // Test 4: Dequeue tasks
    printf("\n4. Testing task dequeue...\n");
    struct task_info_s dequeued_task;
    
    for (int i = 0; i < 3; i++) {
        bool success = task_buffer_dequeue(&buffer, &dequeued_task);
        assert(success);
        printf("   ✓ Dequeued task %d: %s\n", dequeued_task.task_id, dequeued_task.task_name);
        print_task(&dequeued_task);
        printf("\n");
    }
    
    printf("   ✓ Buffer size after dequeuing 3 tasks: %d\n", task_buffer_size(&buffer));
    
    // Test 5: Print buffer state after dequeue
    printf("\n5. Buffer state after dequeue:\n");
    task_buffer_print(&buffer);
    
    // Test 6: Add more tasks to test circular buffer
    printf("\n6. Testing circular buffer behavior...\n");
    struct task_info_s new_tasks[3];
    
    create_test_task(&new_tasks[0], 6, "LogAnalysis", 1);
    create_test_task(&new_tasks[1], 7, "SecurityScan", 3);
    create_test_task(&new_tasks[2], 8, "BackupTask", 2);
    
    for (int i = 0; i < 3; i++) {
        bool success = task_buffer_enqueue(&buffer, &new_tasks[i]);
        assert(success);
        printf("   ✓ Added task %d: %s\n", new_tasks[i].task_id, new_tasks[i].task_name);
    }
    
    printf("   ✓ Buffer size after adding 3 more tasks: %d\n", task_buffer_size(&buffer));
    
    // Test 7: Final buffer state
    printf("\n7. Final buffer state:\n");
    task_buffer_print(&buffer);
    
    // Test 8: Drain the buffer
    printf("\n8. Draining remaining tasks...\n");
    int remaining = task_buffer_size(&buffer);
    for (int i = 0; i < remaining; i++) {
        bool success = task_buffer_dequeue(&buffer, &dequeued_task);
        assert(success);
        printf("   ✓ Drained task %d: %s\n", dequeued_task.task_id, dequeued_task.task_name);
    }
    
    printf("   ✓ Buffer size after draining: %d\n", task_buffer_size(&buffer));
    
    // Test 9: Test buffer bounds
    printf("\n9. Testing buffer capacity...\n");
    printf("   Adding tasks up to buffer capacity (%d)...\n", MAX_TASKS);
    
    for (int i = 0; i < MAX_TASKS; i++) {
        struct task_info_s capacity_task;
        create_test_task(&capacity_task, 100 + i, "CapacityTest", 1);
        bool success = task_buffer_enqueue(&buffer, &capacity_task);
        assert(success);
    }
    
    printf("   ✓ Buffer filled to capacity: %d/%d\n", task_buffer_size(&buffer), MAX_TASKS);
    
    // Test 10: Cleanup
    printf("\n10. Cleanup...\n");
    task_buffer_destroy(&buffer);
    printf("   ✓ Buffer destroyed successfully\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
