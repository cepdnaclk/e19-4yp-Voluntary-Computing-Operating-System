const path = require('path');
const os = require('os');
const net = require('net');
const fs = require('fs');

// Explicitly add the global modules path
const globalModulesPath = path.join(os.homedir(), 'node_global_modules', 'node_modules');
module.paths.unshift(globalModulesPath);

// Try to import canvas for fallback image processing
let createCanvas, loadImage;
try {
  const canvas = require('canvas');
  createCanvas = canvas.createCanvas;
  loadImage = canvas.loadImage;
  console.log('[NODE] Canvas module loaded successfully');
} catch (error) {
  console.log('[NODE] Canvas module not available, using TensorFlow only');
}

const SOCKET_PATH = '/tmp/volcom_unix_socket';

// <--------- EDIT --------->
// Import modules you want here
const tf = require('@tensorflow/tfjs-node');
const cocoSsd = require('@tensorflow-models/coco-ssd');
// const { createCanvas, loadImage } = require('canvas');

// Load model once globally
let modelPromise = cocoSsd.load();
// <--------- END EDIT --------->

// <--------- DO NOT EDIT --------->
// Clean up the socket file if it exists
if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);

const server = net.createServer((socket) => {
  console.log('[NODE] Client connected');

  let buffer = Buffer.alloc(0);
  let isProcessing = false;

  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);
    console.log(`[NODE] Received ${data.length} bytes, total buffer: ${buffer.length} bytes`);
    
    // Try to parse as complete JSON message
    try {
      const bufferString = buffer.toString('utf8');
      const jsonData = JSON.parse(bufferString);
      
      // If we successfully parsed JSON, process it immediately
      console.log('[NODE] Received complete JSON message, processing...');
      if (!isProcessing) {
        processJsonData(socket, jsonData);
      }
    } catch (jsonError) {
      // Not complete JSON yet, or not JSON at all
      // Check if we received the end-of-data marker (fallback for binary data)
      const bufferString = buffer.toString('utf8');
      const endMarkerIndex = bufferString.indexOf('\n<<END_OF_DATA>>\n');
      
      if (endMarkerIndex !== -1) {
        console.log('[NODE] End-of-data marker received, processing buffer...');
        // Extract only the image data (before the marker)
        const imageBuffer = buffer.slice(0, endMarkerIndex);
        
        if (!isProcessing && imageBuffer.length > 0) {
          processData(socket, imageBuffer);
        }
      }
    }
  });

  socket.on('end', () => {
    console.log('[NODE] Client ended connection');
    // Only process if we haven't processed with the end marker
    if (!isProcessing && buffer.length > 0) {
      console.log('[NODE] Processing buffer from connection end (fallback)...');
      processData(socket, buffer);
    }
  });

  async function processData(clientSocket, dataBuffer) {
    if (isProcessing) {
      console.log('[NODE] Already processing, skipping...');
      return;
    }
    
    isProcessing = true;
    console.log(`[NODE] Processing ${dataBuffer.length} bytes of data...`);

    try {
      // Check if the socket is still writable
      if (clientSocket.destroyed || !clientSocket.writable) {
        console.log('[NODE] Socket is not writable, skipping response');
        return;
      }

      const detectionResult = await detectFromBuffer(dataBuffer);
      const response = {
        status: 'success',
        objects: detectionResult.length,
        predictions: detectionResult,
        timestamp: new Date().toISOString()
      };
      console.log('[NODE] Detection completed:', response);
      
      // Send response back to client if socket is still open
      if (!clientSocket.destroyed && clientSocket.writable) {
        clientSocket.write(JSON.stringify(response));
        clientSocket.end(); // End the connection after sending response
      }
    } catch (err) {
      console.error('[NODE] Detection failed:', err);
      const errorResponse = {
        status: 'error',
        message: err.message,
        timestamp: new Date().toISOString()
      };
      
      // Send error response if socket is still open
      if (!clientSocket.destroyed && clientSocket.writable) {
        clientSocket.write(JSON.stringify(errorResponse));
        clientSocket.end();
      }
    } finally {
      isProcessing = false;
    }
  }

  async function processJsonData(clientSocket, jsonData) {
    if (isProcessing) {
      console.log('[NODE] Already processing, skipping...');
      return;
    }
    
    isProcessing = true;
    console.log('[NODE] Processing JSON data:', jsonData.type);

    try {
      // Check if the socket is still writable
      if (clientSocket.destroyed || !clientSocket.writable) {
        console.log('[NODE] Socket is not writable, skipping response');
        return;
      }

      let detectionResult;
      
      if (jsonData.type === 'image_detection' && jsonData.image_data) {
        // Convert base64 back to buffer
        const imageBuffer = Buffer.from(jsonData.image_data, 'base64');
        console.log(`[NODE] Decoded image buffer size: ${imageBuffer.length} bytes`);
        detectionResult = await detectFromBuffer(imageBuffer);
      } else {
        throw new Error('Invalid JSON format. Expected type: "image_detection" with image_data field');
      }
      
      const response = {
        status: 'success',
        objects: detectionResult.length,
        predictions: detectionResult,
        timestamp: new Date().toISOString(),
        original_timestamp: jsonData.timestamp
      };
      console.log('[NODE] Detection completed:', response);
      
      // Send response back to client if socket is still open
      if (!clientSocket.destroyed && clientSocket.writable) {
        clientSocket.write(JSON.stringify(response));
        clientSocket.end(); // End the connection after sending response
      }
    } catch (err) {
      console.error('[NODE] JSON Detection failed:', err);
      const errorResponse = {
        status: 'error',
        message: err.message,
        timestamp: new Date().toISOString()
      };
      
      // Send error response if socket is still open
      if (!clientSocket.destroyed && clientSocket.writable) {
        clientSocket.write(JSON.stringify(errorResponse));
        clientSocket.end();
      }
    } finally {
      isProcessing = false;
    }
  }

  socket.on('error', (err) => {
    console.error('[NODE] Socket error:', err);
  });
});

server.listen(SOCKET_PATH, () => {
  console.log(`[NODE] Listening on Unix socket: ${SOCKET_PATH}`);
});

// Cleanup
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

function shutdown() {
  console.log('[NODE] Shutting down server...');
  server.close(() => {
    if (fs.existsSync(SOCKET_PATH)) fs.unlinkSync(SOCKET_PATH);
    process.exit(0);
  });
}


// <--------- END DO NOT EDIT --------->

// <--------- EDIT --------->
// Your processes function and logics goes here
async function detectFromBuffer(imageBuffer) {
  try {
    console.log('[NODE] Starting object detection...');
    console.log(`[NODE] Buffer size: ${imageBuffer.length} bytes`);
    
    // First, let's check what kind of data we received
    const header = imageBuffer.slice(0, 10);
    console.log('[NODE] Buffer header:', header);
    
    // Try to parse as JSON first (in case it's JSON data instead of image)
    let imageTensor;
    try {
      const jsonString = imageBuffer.toString('utf8');
      console.log('[NODE] Attempting to parse as JSON:', jsonString.substring(0, 100) + '...');
      const jsonData = JSON.parse(jsonString);
      
      // If it's JSON data, we might need to extract image path or data
      if (jsonData.image || jsonData.imagePath || jsonData.data) {
        console.log('[NODE] Received JSON data, processing...');
        // For now, return a mock response for JSON input
        return [{
          class: 'data_processing',
          score: 1.0,
          bbox: [0, 0, 100, 100]
        }];
      }
    } catch (jsonError) {
      // Not JSON, continue with image processing
      console.log('[NODE] Not JSON data, treating as image buffer');
    }
    
    // Try to decode as image
    try {
      imageTensor = tf.node.decodeImage(imageBuffer, 3); // Force 3 channels (RGB)
      console.log('[NODE] Image tensor shape:', imageTensor.shape);
    } catch (decodeError) {
      console.log('[NODE] Failed to decode as image, trying alternative approach...');
      
      // Try to save buffer to file and read it back
      const tempPath = `/tmp/temp_image_${Date.now()}.jpg`;
      fs.writeFileSync(tempPath, imageBuffer);
      
      try {
        if (createCanvas && loadImage) {
          // Try to load using canvas
          const image = await loadImage(tempPath);
          const canvas = createCanvas(image.width, image.height);
          const ctx = canvas.getContext('2d');
          ctx.drawImage(image, 0, 0);
          
          // Convert canvas to tensor
          imageTensor = tf.browser.fromPixels(canvas);
          console.log('[NODE] Image loaded via canvas, tensor shape:', imageTensor.shape);
        } else {
          throw new Error('Canvas not available');
        }
        
        // Clean up temp file
        fs.unlinkSync(tempPath);
      } catch (canvasError) {
        // Clean up temp file
        if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
        throw new Error(`Failed to decode image: ${decodeError.message}. Canvas fallback failed: ${canvasError.message}`);
      }
    }
    
    // Load the model
    const model = await modelPromise;
    console.log('[NODE] Model loaded successfully');
    
    // Perform object detection directly on the tensor
    const predictions = await model.detect(imageTensor);
    console.log('[NODE] Raw predictions:', predictions);
    
    // Clean up tensor
    imageTensor.dispose();
    
    // Format predictions for better readability
    const formattedPredictions = predictions.map(pred => ({
      class: pred.class,
      score: Math.round(pred.score * 100) / 100, // Round to 2 decimal places
      bbox: pred.bbox.map(coord => Math.round(coord))
    }));
    
    console.log('[NODE] Detection complete. Found', formattedPredictions.length, 'objects');
    return formattedPredictions;
    
  } catch (error) {
    console.error('[NODE] Error in detectFromBuffer:', error);
    throw error;
  }
}
// <--------- END EDIT --------->