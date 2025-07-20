const fs = require('fs');
const net = require('net');
const SOCKET_PATH = '/tmp/volcom_unix_socket';

const imagePath = 'frames/frame_00002.jpg';

// Read and analyze the image
const imageBuffer = fs.readFileSync(imagePath);
console.log('Original image buffer size:', imageBuffer.length);
console.log('Original image buffer header:', imageBuffer.slice(0, 20));

const client = net.createConnection(SOCKET_PATH, () => {
  console.log('Connected to object detection server');
  console.log('Original image buffer size:', imageBuffer.length);
  
  // Convert image buffer to base64 and send as JSON
  const imageBase64 = imageBuffer.toString('base64');
  const jsonMessage = {
    type: 'image_detection',
    image_data: imageBase64,
    format: 'jpg',
    timestamp: new Date().toISOString()
  };
  
  const jsonString = JSON.stringify(jsonMessage);
  console.log('Sending JSON message of size:', jsonString.length);
  
  // Send the complete JSON message
  client.write(jsonString);
  console.log('JSON message sent, waiting for response...');
});

client.on('data', (data) => {
  console.log('Received response length:', data.length);
  console.log('Received response:', data.toString());
  
  // Parse and display the response nicely
  try {
    const response = JSON.parse(data.toString());
    console.log('Parsed response:');
    console.log('Status:', response.status);
    if (response.status === 'success') {
      console.log('Objects detected:', response.objects);
      console.log('Predictions:', JSON.stringify(response.predictions, null, 2));
    } else {
      console.log('Error message:', response.message);
    }
  } catch (parseError) {
    console.log('Could not parse response as JSON');
  }
  
  // Close the client after receiving the response
  client.destroy();
});

client.on('end', () => {
  console.log('Connection ended');
});

client.on('error', (err) => {
  console.error('Client error:', err);
});
