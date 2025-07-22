const fs = require('fs');
const net = require('net');
const path = require('path');

const SOCKET_PATH = '/tmp/volcom_unix_socket';

class JSONFrameProcessor {
  constructor(inputFolder, outputFolder) {
    this.inputFolder = inputFolder;
    this.outputFolder = outputFolder;
    this.processedCount = 0;
    this.totalFrames = 0;
  }

  async processJSONFrame(jsonFilePath) {
    return new Promise((resolve, reject) => {
      // Read the JSON frame
      const frameData = JSON.parse(fs.readFileSync(jsonFilePath, 'utf8'));
      
      console.log(`\nProcessing: ${path.basename(jsonFilePath)}`);
      console.log(`Frame ${frameData.frameNumber}, Size: ${frameData.size} bytes`);

      // Create message for object detection server
      const jsonMessage = {
        type: 'image_detection',
        image_data: frameData.imageData, // Already base64 encoded
        format: frameData.format,
        timestamp: new Date().toISOString(),
        originalFrame: frameData.frameNumber,
        originalFileName: frameData.originalFileName
      };

      const client = net.createConnection(SOCKET_PATH, () => {
        client.write(JSON.stringify(jsonMessage));
        console.log('JSON message sent to object detection server...');
      });

      client.responseBuffer = Buffer.alloc(0);

      client.on('data', (data) => {
        client.responseBuffer = Buffer.concat([client.responseBuffer, data]);
        
        try {
          const response = JSON.parse(client.responseBuffer.toString());
          
          console.log('✅ Successfully processed frame');
          console.log(`Status: ${response.status}`);
          
          if (response.status === 'success') {
            console.log(`Objects detected: ${response.objects}`);
            
            // Create enhanced JSON with detection results
            const enhancedFrameData = {
              ...frameData, // Keep original frame data
              detection: {
                timestamp: response.timestamp,
                objects: response.objects,
                predictions: response.predictions,
                processingTime: new Date(response.timestamp) - new Date(frameData.timestamp)
              }
            };

            // Add annotated image if present
            if (response.annotated_image) {
              enhancedFrameData.annotatedImageData = response.annotated_image;
              console.log(`📁 Annotated image data included`);
            }

            // Save enhanced JSON
            const outputFileName = path.basename(jsonFilePath, '.json') + '_detected.json';
            const outputPath = path.join(this.outputFolder, outputFileName);
            fs.writeFileSync(outputPath, JSON.stringify(enhancedFrameData, null, 2));
            
            console.log(`💾 Enhanced frame data saved to: ${outputFileName}`);
          } else {
            console.log(`❌ Detection failed: ${response.message}`);
            
            // Save frame with error info
            const errorFrameData = {
              ...frameData,
              detection: {
                timestamp: new Date().toISOString(),
                status: 'error',
                error: response.message
              }
            };
            
            const outputFileName = path.basename(jsonFilePath, '.json') + '_error.json';
            const outputPath = path.join(this.outputFolder, outputFileName);
            fs.writeFileSync(outputPath, JSON.stringify(errorFrameData, null, 2));
          }
          
          this.processedCount++;
          console.log(`📈 Progress: ${this.processedCount}/${this.totalFrames} (${Math.round(this.processedCount/this.totalFrames*100)}%)`);
          
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

  async processAllJSONFrames() {
    console.log('🚀 Starting JSON frame processing with object detection...');
    
    // Create output folder
    if (!fs.existsSync(this.outputFolder)) {
      fs.mkdirSync(this.outputFolder, { recursive: true });
    }

    // Get all JSON frame files
    const jsonFiles = fs.readdirSync(this.inputFolder)
      .filter(file => file.startsWith('frame_') && file.endsWith('.json'))
      .sort();

    this.totalFrames = jsonFiles.length;
    console.log(`Found ${this.totalFrames} JSON frames to process`);

    if (this.totalFrames === 0) {
      throw new Error('No JSON frame files found in input folder');
    }

    // Process frames sequentially to avoid overwhelming the server
    for (let i = 0; i < jsonFiles.length; i++) {
      try {
        const jsonFilePath = path.join(this.inputFolder, jsonFiles[i]);
        await this.processJSONFrame(jsonFilePath);
        
        // Small delay between requests
        if (i < jsonFiles.length - 1) {
          await new Promise(resolve => setTimeout(resolve, 100));
        }
      } catch (error) {
        console.error(`❌ Failed to process ${jsonFiles[i]}: ${error.message}`);
        // Continue with next frame instead of stopping
      }
    }
    
    console.log('\n🎉 JSON frame processing complete!');
    console.log(`📊 Successfully processed: ${this.processedCount}/${this.totalFrames} frames`);
    
    return this.processedCount;
  }
}

// Function to extract annotated images from processed JSON frames
function extractAnnotatedImages(processedJsonFolder, outputImageFolder) {
  if (!fs.existsSync(processedJsonFolder)) {
    throw new Error(`Processed JSON folder not found: ${processedJsonFolder}`);
  }

  if (!fs.existsSync(outputImageFolder)) {
    fs.mkdirSync(outputImageFolder, { recursive: true });
  }

  const processedFiles = fs.readdirSync(processedJsonFolder)
    .filter(f => f.endsWith('_detected.json'))
    .sort();

  console.log(`Extracting annotated images from ${processedFiles.length} processed frames...`);

  let extractedCount = 0;

  for (const jsonFile of processedFiles) {
    const jsonPath = path.join(processedJsonFolder, jsonFile);
    
    try {
      const frameData = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
      
      if (frameData.annotatedImageData) {
        const annotatedBuffer = Buffer.from(frameData.annotatedImageData, 'base64');
        const imageFileName = frameData.originalFileName.replace('.jpg', '_annotated.jpg');
        const imagePath = path.join(outputImageFolder, imageFileName);
        
        fs.writeFileSync(imagePath, annotatedBuffer);
        extractedCount++;
      }
    } catch (error) {
      console.error(`Error processing ${jsonFile}: ${error.message}`);
    }
  }

  console.log(`✅ Extracted ${extractedCount} annotated images to: ${outputImageFolder}`);
  return extractedCount;
}

// Usage
const command = process.argv[2];
const inputFolder = process.argv[3];
const outputFolder = process.argv[4];

if (!command) {
  console.log('Usage:');
  console.log('  Process JSON frames with object detection:');
  console.log('    node json-frame-processor.js process <json_frames_folder> [output_folder]');
  console.log('');
  console.log('  Extract annotated images from processed JSON:');
  console.log('    node json-frame-processor.js extract <processed_json_folder> [output_image_folder]');
  console.log('');
  console.log('Examples:');
  console.log('  node json-frame-processor.js process json_frames processed_frames');
  console.log('  node json-frame-processor.js extract processed_frames annotated_images');
  console.log('');
  console.log('⚠️  Make sure the object detection server is running!');
  console.log('   Start it with: node object-detection.js');
  process.exit(1);
}

if (command === 'process') {
  if (!inputFolder) {
    console.error('Error: Input JSON frames folder is required');
    process.exit(1);
  }
  
  const output = outputFolder || 'processed_frames';
  
  // Check if object detection server is running
  const testClient = net.createConnection(SOCKET_PATH);
  testClient.on('connect', () => {
    testClient.destroy();
    
    // Server is running, start processing
    const processor = new JSONFrameProcessor(inputFolder, output);
    processor.processAllJSONFrames()
      .catch((error) => {
        console.error('Processing failed:', error.message);
        process.exit(1);
      });
  });

  testClient.on('error', () => {
    console.error('❌ Error: Object detection server is not running!');
    console.log('Please start the server first:');
    console.log('  node object-detection.js');
    process.exit(1);
  });
  
} else if (command === 'extract') {
  if (!inputFolder) {
    console.error('Error: Processed JSON folder is required');
    process.exit(1);
  }
  
  const output = outputFolder || 'annotated_images';
  
  try {
    extractAnnotatedImages(inputFolder, output);
  } catch (error) {
    console.error('Error:', error.message);
    process.exit(1);
  }
  
} else {
  console.error('Error: Unknown command. Use "process" or "extract"');
  process.exit(1);
}

module.exports = {
  JSONFrameProcessor,
  extractAnnotatedImages
};
