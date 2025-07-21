const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const { promisify } = require('util');

async function extractFramesToJSON(videoPath, outputFolder) {
  // Check if video file exists
  if (!fs.existsSync(videoPath)) {
    throw new Error(`Video file not found: ${videoPath}`);
  }

  // Create output folder if it doesn't exist
  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  // First, extract frames as temporary JPEG files
  const tempFolder = path.join(outputFolder, 'temp_frames');
  if (!fs.existsSync(tempFolder)) {
    fs.mkdirSync(tempFolder, { recursive: true });
  }

  console.log(`Extracting frames from ${videoPath}...`);

  return new Promise((resolve, reject) => {
    // Extract all frames at original frame rate to temporary folder
    const ffmpegArgs = [
      '-i', videoPath,
      '-y', // Overwrite output files
      '-q:v', '2', // High quality
      path.join(tempFolder, 'frame_%05d.jpg')
    ];
    
    const ffmpeg = spawn('ffmpeg', ffmpegArgs);

    let stderr = '';

    ffmpeg.stderr.on('data', (data) => {
      stderr += data.toString();
      // Show progress
      if (data.toString().includes('frame=')) {
        process.stdout.write('.');
      }
    });

    ffmpeg.on('close', async (code) => {
      if (code === 0) {
        console.log('\nFrame extraction complete! Converting to JSON...');
        
        try {
          // Get all extracted frame files
          const frameFiles = fs.readdirSync(tempFolder)
            .filter(f => f.startsWith('frame_') && f.endsWith('.jpg'))
            .sort();

          console.log(`Found ${frameFiles.length} frames to convert`);

          // Convert each frame to JSON
          for (let i = 0; i < frameFiles.length; i++) {
            const frameFile = frameFiles[i];
            const framePath = path.join(tempFolder, frameFile);
            
            // Read the image buffer
            const imageBuffer = fs.readFileSync(framePath);
            
            // Convert to base64
            const imageBase64 = imageBuffer.toString('base64');
            
            // Create JSON object
            const frameData = {
              frameNumber: i + 1,
              originalFileName: frameFile,
              timestamp: new Date().toISOString(),
              format: 'jpg',
              size: imageBuffer.length,
              imageData: imageBase64
            };
            
            // Save as JSON file
            const jsonFileName = frameFile.replace('.jpg', '.json');
            const jsonPath = path.join(outputFolder, jsonFileName);
            fs.writeFileSync(jsonPath, JSON.stringify(frameData, null, 2));
            
            // Show progress
            if ((i + 1) % 50 === 0 || i === frameFiles.length - 1) {
              process.stdout.write(`\rConverted ${i + 1}/${frameFiles.length} frames to JSON`);
            }
          }
          
          console.log('\n✅ JSON conversion complete!');
          
          // Clean up temporary folder
          console.log('🧹 Cleaning up temporary files...');
          fs.rmSync(tempFolder, { recursive: true, force: true });
          
          console.log(`📁 ${frameFiles.length} JSON files saved in: ${outputFolder}`);
          resolve(frameFiles.length);
          
        } catch (error) {
          reject(new Error(`Error converting frames to JSON: ${error.message}`));
        }
      } else {
        console.error('FFmpeg stderr:', stderr);
        reject(new Error(`FFmpeg process exited with code ${code}`));
      }
    });

    ffmpeg.on('error', (err) => {
      reject(new Error(`Failed to start FFmpeg: ${err.message}`));
    });
  });
}

// Function to read a JSON frame back (utility function)
function readJSONFrame(jsonFilePath) {
  if (!fs.existsSync(jsonFilePath)) {
    throw new Error(`JSON file not found: ${jsonFilePath}`);
  }
  
  const jsonData = JSON.parse(fs.readFileSync(jsonFilePath, 'utf8'));
  
  // Convert base64 back to buffer if needed
  const imageBuffer = Buffer.from(jsonData.imageData, 'base64');
  
  return {
    frameNumber: jsonData.frameNumber,
    originalFileName: jsonData.originalFileName,
    timestamp: jsonData.timestamp,
    format: jsonData.format,
    size: jsonData.size,
    imageBuffer: imageBuffer,
    imageBase64: jsonData.imageData
  };
}

// Function to convert JSON frames back to images (utility function)
async function convertJSONFramesToImages(jsonFolder, outputFolder) {
  if (!fs.existsSync(jsonFolder)) {
    throw new Error(`JSON folder not found: ${jsonFolder}`);
  }

  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  const jsonFiles = fs.readdirSync(jsonFolder)
    .filter(f => f.endsWith('.json') && f.startsWith('frame_'))
    .sort();

  console.log(`Converting ${jsonFiles.length} JSON files back to images...`);

  for (let i = 0; i < jsonFiles.length; i++) {
    const jsonFile = jsonFiles[i];
    const jsonPath = path.join(jsonFolder, jsonFile);
    
    try {
      const frameData = readJSONFrame(jsonPath);
      const outputImagePath = path.join(outputFolder, frameData.originalFileName);
      
      fs.writeFileSync(outputImagePath, frameData.imageBuffer);
      
      if ((i + 1) % 50 === 0 || i === jsonFiles.length - 1) {
        process.stdout.write(`\rConverted ${i + 1}/${jsonFiles.length} JSON files back to images`);
      }
    } catch (error) {
      console.error(`\nError converting ${jsonFile}: ${error.message}`);
    }
  }

  console.log(`\n✅ Conversion complete! Images saved in: ${outputFolder}`);
}

// Usage
const command = process.argv[2];
const videoPath = process.argv[3];
const outputFolder = process.argv[4] || 'json_frames';

// Show usage if no command provided
if (!command) {
  console.log('Usage:');
  console.log('  Extract video to JSON frames:');
  console.log('    node video-to-json.js extract <video_file> [output_folder]');
  console.log('');
  console.log('  Convert JSON frames back to images:');
  console.log('    node video-to-json.js convert <json_folder> [output_folder]');
  console.log('');
  console.log('Examples:');
  console.log('  node video-to-json.js extract input.mp4 json_frames');
  console.log('  node video-to-json.js convert json_frames restored_images');
  console.log('');
  console.log('JSON Frame Structure:');
  console.log('  {');
  console.log('    "frameNumber": 1,');
  console.log('    "originalFileName": "frame_00001.jpg",');
  console.log('    "timestamp": "2025-01-01T00:00:00.000Z",');
  console.log('    "format": "jpg",');
  console.log('    "size": 123456,');
  console.log('    "imageData": "base64-encoded-image-data..."');
  console.log('  }');
  process.exit(1);
}

if (command === 'extract') {
  if (!videoPath) {
    console.error('Error: Video file path is required');
    process.exit(1);
  }
  
  extractFramesToJSON(videoPath, outputFolder).catch((error) => {
    console.error('Error:', error.message);
    process.exit(1);
  });
  
} else if (command === 'convert') {
  if (!videoPath) { // Using videoPath as jsonFolder in this context
    console.error('Error: JSON folder path is required');
    process.exit(1);
  }
  
  const jsonFolder = videoPath;
  const imageOutputFolder = outputFolder || 'restored_images';
  
  convertJSONFramesToImages(jsonFolder, imageOutputFolder).catch((error) => {
    console.error('Error:', error.message);
    process.exit(1);
  });
  
} else {
  console.error('Error: Unknown command. Use "extract" or "convert"');
  process.exit(1);
}

// Export functions for use in other modules
module.exports = {
  extractFramesToJSON,
  readJSONFrame,
  convertJSONFramesToImages
};
