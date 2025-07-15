// Data processing script for volcom

function processDataPoint(data) {
    try {
        // Parse the input data (JSON format)
        const inputData = JSON.parse(data);
        
        const processed = {
            id: inputData.id || 'unknown',
            originalValue: inputData.value || 0,
            processedValue: (inputData.value || 0) * 2 + 10, 
            squared: Math.pow(inputData.value || 0, 2),
            timestamp: new Date().toISOString(),
            processedBy: process.pid
        };
        
        if (inputData.type === 'sensor') {
            processed.normalized = Math.min(Math.max(inputData.value / 100, 0), 1);
        } else if (inputData.type === 'calculation') {
            processed.fibonacci = fibonacci(Math.min(inputData.value, 20)); // Limit to prevent overflow
        }
        
        return processed;
    } catch (error) {
        return {
            error: true,
            message: error.message,
            originalData: data,
            processedBy: process.pid
        };
    }
}

function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

function main() {
    console.log(`[PID ${process.pid}] Data processor started`);
    
    // Get data from command line arguments
    const dataArgs = process.argv.slice(2);
    
    if (dataArgs.length === 0) {
        console.error('No data provided');
        process.exit(1);
    }
    
    const results = [];
    
    dataArgs.forEach((dataPoint, index) => {
        console.log(`Processing data point ${index + 1}: ${dataPoint}`);
        const result = processDataPoint(dataPoint);
        results.push(result);
        
        console.log(`Result ${index + 1}:`, JSON.stringify(result, null, 2));
    });
    

    // console.log(`\n=== Processing Summary ===`);
    // console.log(`Total data points processed: ${results.length}`);
    // console.log(`Successful: ${results.filter(r => !r.error).length}`);
    // console.log(`Errors: ${results.filter(r => r.error).length}`);
    // console.log(`Processed by PID: ${process.pid}`);
    
    process.exit(0);
}

main();
