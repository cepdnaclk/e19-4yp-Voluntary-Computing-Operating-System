const net = require('net');
const fs = require('fs');
const path = require('path');

const SOCKET_PATH = '/tmp/volcom_unix_socket';

// Clean up the socket file if it exists
if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);

const server = net.createServer((socket) => {
    console.log('Client connected to Unix socket server');
    
    let buffer = Buffer.alloc(0);

    socket.on('data', (data) => {
        buffer = Buffer.concat([buffer, data]);
        
        // Check if we have a complete message (assuming text messages)
        const messageStr = buffer.toString('utf8');
        if (messageStr.includes('\n') || messageStr.includes('\0')) {
            const messages = messageStr.split(/[\n\0]/);
            
            for (let i = 0; i < messages.length - 1; i++) {
                if (messages[i].trim()) {
                    processMessage(messages[i].trim(), socket);
                }
            }
            
            // Keep the last incomplete message in buffer
            buffer = Buffer.from(messages[messages.length - 1]);
        } else {
            // Process the complete buffer as a single message
            processMessage(messageStr, socket);
            buffer = Buffer.alloc(0);
        }
    });

    socket.on('end', () => {
        console.log('Client disconnected from Unix socket server');
    });

    socket.on('error', (err) => {
        console.error('Socket error:', err);
    });
});

server.listen(SOCKET_PATH, () => {
    console.log(`Unix Socket Server listening on ${SOCKET_PATH}`);
});

function processMessage(message, socket) {
    console.log('Received message:', message);
    
    try {
        // Try to parse as JSON first
        const data = JSON.parse(message);
        
        if (data.status === 'success') {
            console.log('Processing success message:', data.message);
            
            // Send acknowledgment back to client
            const response = {
                status: 'acknowledged',
                message: 'Success message received and processed',
                timestamp: new Date().toISOString()
            };
            
            socket.write(JSON.stringify(response));
            return;
        }
        
        // Process other JSON messages
        processJsonData(data, socket);
        
    } catch (e) {
        // Not JSON, treat as binary data or plain text
        processBinaryData(message, socket);
    }
}

function processJsonData(data, socket) {
    console.log('Processing JSON data:', data);
    
    const response = {
        status: 'received',
        message: 'JSON data processed successfully',
        timestamp: new Date().toISOString()
    };
    
    socket.write(JSON.stringify(response));
}

function processBinaryData(dataPoint, socket) {
    // Process binary data (original functionality)
    const name = `img_${Date.now()}.bin`;
    
    try {
        fs.writeFileSync(name, dataPoint);
        console.log(`Saved ${name}`);
        
        // Send success message to Unix socket (if there's a client connected)
        const successMessage = {
            status: 'success',
            message: 'Data saved successfully',
            timestamp: new Date().toISOString(),
            file: name,
            size: dataPoint.length
        };
        
        console.log('Sending success message:', successMessage);
        
        // Send success message back to the client
        socket.write(JSON.stringify(successMessage));
        
    } catch (error) {
        console.error('Error saving file:', error);
        
        const errorMessage = {
            status: 'error',
            message: 'Failed to save data',
            timestamp: new Date().toISOString(),
            error: error.message
        };
        
        socket.write(JSON.stringify(errorMessage));
    }
}

// Handle server shutdown
process.on('SIGINT', () => {
    console.log('\nShutting down Unix socket server...');
    server.close(() => {
        if (fs.existsSync(SOCKET_PATH)) {
            fs.unlinkSync(SOCKET_PATH);
        }
        process.exit(0);
    });
});

process.on('SIGTERM', () => {
    console.log('\nShutting down Unix socket server...');
    server.close(() => {
        if (fs.existsSync(SOCKET_PATH)) {
            fs.unlinkSync(SOCKET_PATH);
        }
        process.exit(0);
    });
});