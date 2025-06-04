// Simple script to test cgroup limits
console.log(`[PID ${process.pid}] Node.js script started`);

// Test memory allocation
function testMemory() {
    const memoryHog = [];
    const MB = 1024 * 1024;
    let allocated = 0;
    const targetMB = 200; // Try to allocate 200MB

    console.log(`Testing memory limit (attempting ${targetMB}MB allocation)...`);
    
    try {
        while (allocated < targetMB) {
            memoryHog.push(Buffer.alloc(MB, 'x')); // Allocate 1MB
            allocated++;
            process.stdout.write(`\rAllocated: ${allocated}MB`);
        }
        console.log("\nMemory test passed (no limits enforced?)");
    } catch (err) {
        console.log(`\nMemory limit hit at ${allocated}MB!`, err.message);
    }
}

// Test CPU usage
function testCPU() {
    console.log("Starting CPU stress test (10 seconds)...");
    const start = Date.now();
    let count = 0;
    
    while (Date.now() - start < 10000) { // Run for 10 seconds
        Math.sqrt(Math.random() * 1000000); // Waste CPU cycles
        count++;
    }
    
    console.log(`Completed ${count.toLocaleString()} calculations`);
}

// Run tests with delay between them
setTimeout(testMemory, 1000);
setTimeout(testCPU, 3000);

// Keep process alive
setInterval(() => {}, 1000);