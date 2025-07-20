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
  // Buffer the response data in case it comes in multiple chunks
  if (!client.responseBuffer) {
    client.responseBuffer = Buffer.alloc(0);
  }
  
  client.responseBuffer = Buffer.concat([client.responseBuffer, data]);
  console.log(`Received ${data.length} bytes, total buffered: ${client.responseBuffer.length} bytes`);
  
  // Try to parse the complete JSON response
  try {
    const responseString = client.responseBuffer.toString();
    const response = JSON.parse(responseString);
    
    console.log('Successfully parsed complete JSON response');
    console.log('Status:', response.status);
    if (response.status === 'success') {
      console.log('Objects detected:', response.objects);
      console.log('Predictions:', JSON.stringify(response.predictions, null, 2));
      
      // Handle annotated image if present
      if (response.annotated_image) {
        const annotatedBuffer = Buffer.from(response.annotated_image, 'base64');
        const outputPath = 'detected_frames/annotated_' + imagePath.split('/').pop();
        
        // Create directory if it doesn't exist
        if (!fs.existsSync('detected_frames')) {
          fs.mkdirSync('detected_frames');
        }
        
        fs.writeFileSync(outputPath, annotatedBuffer);
        console.log(`Annotated image saved to: ${outputPath}`);
        console.log(`Annotated image size: ${annotatedBuffer.length} bytes`);
      } else {
        console.log('No annotated image received');
      }
    } else {
      console.log('Error message:', response.message);
    }
    
    // Close the client after successful parsing
    client.destroy();
  } catch (parseError) {
    // JSON is not complete yet, wait for more data
    console.log('JSON not complete yet, waiting for more data...');
  }
});

client.on('end', () => {
  console.log('Connection ended');
});

client.on('error', (err) => {
  console.error('Client error:', err);
});
