const fs = require('fs');
const net = require('net');
const path = require('path');

const SOCKET_PATH = '/tmp/volcom_unix_socket';
const inputFolder = 'temp_frames'; 
const outputFolder = 'detected_frames';

// Get all image files from input folder
const files = fs.readdirSync(inputFolder).filter(file => 
    ['.jpg', '.jpeg', '.png'].includes(path.extname(file).toLowerCase())
).sort(); // Sort to process in order

console.log(`Found ${files.length} images to process`);

// Create output directory if it doesn't exist
if (!fs.existsSync(outputFolder)) {
  fs.mkdirSync(outputFolder, { recursive: true });
}

let currentIndex = 0;
let processedCount = 0;

async function processImage(filePath) {
  return new Promise((resolve, reject) => {
    const imageBuffer = fs.readFileSync(filePath);
    console.log(`\nProcessing: ${path.basename(filePath)}`);
    console.log(`Image buffer size: ${imageBuffer.length} bytes`);

    // Convert image buffer to base64 and send as JSON
    const imageBase64 = imageBuffer.toString('base64');
    const jsonMessage = {
      type: 'image_detection',
      image_data: imageBase64,
      format: path.extname(filePath).toLowerCase().replace('.', ''),
      timestamp: new Date().toISOString()
    };
    
    const jsonString = JSON.stringify(jsonMessage);
    console.log(`Sending JSON message of size: ${jsonString.length} bytes`);
    
    const client = net.createConnection(SOCKET_PATH, () => {
      client.write(jsonString);
      console.log('JSON message sent, waiting for response...');
    });

    client.responseBuffer = Buffer.alloc(0);

    client.on('data', (data) => {
      client.responseBuffer = Buffer.concat([client.responseBuffer, data]);
      console.log(`Received ${data.length} bytes, total buffered: ${client.responseBuffer.length} bytes`);
      
      try {
        const responseString = client.responseBuffer.toString();
        const response = JSON.parse(responseString);
        
        console.log('✅ Successfully parsed complete JSON response');
        console.log(`Status: ${response.status}`);
        
        if (response.status === 'success') {
          console.log(`Objects detected: ${response.objects}`);
          
          // Handle annotated image if present
          if (response.annotated_image) {
            const annotatedBuffer = Buffer.from(response.annotated_image, 'base64');
            const fileName = path.basename(filePath);
            const outputPath = path.join(outputFolder, `annotated_${fileName}`);
            
            fs.writeFileSync(outputPath, annotatedBuffer);
            console.log(`📁 Annotated image saved to: ${outputPath}`);
            console.log(`📊 Annotated image size: ${annotatedBuffer.length} bytes`);
          } else {
            console.log('⚠️  No annotated image received');
          }
        } else {
          console.log(`❌ Error: ${response.message}`);
        }
        
        processedCount++;
        console.log(`📈 Progress: ${processedCount}/${files.length} (${Math.round(processedCount/files.length*100)}%)`);
        
        client.destroy();
        resolve(response);
      } catch (parseError) {
        console.log('⏳ JSON not complete yet, waiting for more data...');
      }
    });

    client.on('error', (err) => {
      console.error(`❌ Client error: ${err.message}`);
      reject(err);
    });

    client.on('end', () => {
      reject(new Error('Connection ended unexpectedly'));
    });
  });
}

async function processAllImages() {
  console.log('🚀 Starting batch image processing...');
  
  for (let i = 0; i < files.length; i++) {
    try {
      const filePath = path.join(inputFolder, files[i]);
      await processImage(filePath);
      
      // Small delay between requests to avoid overwhelming the server
      if (i < files.length - 1) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }
    } catch (error) {
      console.error(`❌ Failed to process ${files[i]}: ${error.message}`);
      // Continue with next image instead of stopping
    }
  }
  
  console.log('\n🎉 Batch processing complete!');
  console.log(`📊 Successfully processed: ${processedCount}/${files.length} images`);
}

// Start processing
processAllImages().catch((error) => {
  console.error('💥 Fatal error:', error.message);
  process.exit(1);
});
