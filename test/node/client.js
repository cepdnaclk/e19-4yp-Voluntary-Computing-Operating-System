const fs = require('fs');
const net = require('net');
const SOCKET_PATH = '/tmp/volcom_unix_socket';

const imagePath = 'frames/frame_00001.jpg'; // Path to the image file
const imageBuffer = fs.readFileSync(imagePath);

const client = net.createConnection(SOCKET_PATH, () => {
  console.log('Connected to object detection server');
  client.write(imageBuffer);
  console.log('Sent image data, waiting for response...');
});

client.on('data', (data) => {
  const response = JSON.parse(data.toString());
  console.log('Object detection results:', response);
  client.end(); // Close connection after receiving response
});

client.on('end', () => {
  console.log('Connection ended');
});

client.on('error', (err) => {
  console.error('Client error:', err);
});
