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
  console.log('Sending image buffer of size:', imageBuffer.length);
  
  // Send the complete image at once instead of chunking
  client.write(imageBuffer);
  console.log('All data sent, ending connection');
  client.end();
});

let responseData = Buffer.alloc(0);

client.on('data', (data) => {
  responseData = Buffer.concat([responseData, data]);
  console.log('Received more data, total length:', responseData.length);
});

client.on('end', () => {
  console.log('Connection ended, processing response...');
  if (responseData.length > 0) {
    try {
      const response = JSON.parse(responseData.toString());
      console.log('Parsed response:');
      console.log('Status:', response.status);
      if (response.status === 'success') {
        console.log('Objects detected:', response.objects);
        console.log('Predictions:');
        response.predictions.forEach((pred, index) => {
          console.log(`  ${index + 1}. ${pred.class} (${(pred.score * 100).toFixed(1)}%) at [${pred.bbox.join(', ')}]`);
        });
      } else {
        console.log('Error message:', response.message);
      }
    } catch (parseError) {
      console.log('Could not parse response as JSON:', responseData.toString());
    }
  } else {
    console.log('No response received');
  }
});

client.on('error', (err) => {
  console.error('Client error:', err);
});
