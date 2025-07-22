const path = require('path');
const fs = require('fs');
const net = require('net');
const { spawn } = require('child_process');

const SOCKET_PATH = '/tmp/volcom_unix_socket';

class VideoObjectDetection {
  constructor() {
    this.framesFolder = 'temp_frames';
    this.outputFolder = 'detected_frames';
    this.processedCount = 0;
    this.totalFrames = 0;
  }

  async extractFrames(videoPath) {
    console.log('📹 Step 1: Extracting frames from video...');
    
    // Create frames folder
    if (!fs.existsSync(this.framesFolder)) {
      fs.mkdirSync(this.framesFolder, { recursive: true });
    }

    return new Promise((resolve, reject) => {
      const ffmpeg = spawn('ffmpeg', [
        '-i', videoPath,
        '-y', // Overwrite output files
        '-q:v', '2', // High quality
        path.join(this.framesFolder, 'frame_%05d.jpg')
      ]);

      ffmpeg.stderr.on('data', (data) => {
        if (data.toString().includes('frame=')) {
          process.stdout.write('.');
        }
      });

      ffmpeg.on('close', (code) => {
        if (code === 0) {
          const files = fs.readdirSync(this.framesFolder).filter(f => f.startsWith('frame_'));
          this.totalFrames = files.length;
          console.log(`\n✅ Extracted ${this.totalFrames} frames`);
          resolve(files);
        } else {
          reject(new Error(`FFmpeg failed with code ${code}`));
        }
      });
    });
  }

  async processFrame(framePath) {
    return new Promise((resolve, reject) => {
      const imageBuffer = fs.readFileSync(framePath);
      
      const jsonMessage = {
        type: 'image_detection',
        image_data: imageBuffer.toString('base64'),
        format: 'jpg',
        timestamp: new Date().toISOString()
      };

      const client = net.createConnection(SOCKET_PATH, () => {
        client.write(JSON.stringify(jsonMessage));
      });

      client.responseBuffer = Buffer.alloc(0);

      client.on('data', (data) => {
        client.responseBuffer = Buffer.concat([client.responseBuffer, data]);
        
        try {
          const response = JSON.parse(client.responseBuffer.toString());
          
          if (response.annotated_image) {
            const annotatedBuffer = Buffer.from(response.annotated_image, 'base64');
            const outputPath = path.join(this.outputFolder, path.basename(framePath));
            fs.writeFileSync(outputPath, annotatedBuffer);
            
            this.processedCount++;
            process.stdout.write(`\r🔍 Processing frames: ${this.processedCount}/${this.totalFrames} (${Math.round(this.processedCount/this.totalFrames*100)}%)`);
            
            client.destroy();
            resolve(outputPath);
          }
        } catch (parseError) {
          // JSON not complete yet, wait for more data
        }
      });

      client.on('error', (err) => {
        reject(err);
      });

      client.on('end', () => {
        reject(new Error('Connection ended unexpectedly'));
      });
    });
  }

  async processAllFrames() {
    console.log('\n🤖 Step 2: Processing frames with object detection...');
    
    // Create output folder
    if (!fs.existsSync(this.outputFolder)) {
      fs.mkdirSync(this.outputFolder, { recursive: true });
    }

    const frameFiles = fs.readdirSync(this.framesFolder)
      .filter(f => f.startsWith('frame_'))
      .sort();

    // Process frames in batches to avoid overwhelming the server
    const batchSize = 5;
    const processedFrames = [];

    for (let i = 0; i < frameFiles.length; i += batchSize) {
      const batch = frameFiles.slice(i, i + batchSize);
      const batchPromises = batch.map(frameFile => {
        const framePath = path.join(this.framesFolder, frameFile);
        return this.processFrame(framePath);
      });

      try {
        const batchResults = await Promise.all(batchPromises);
        processedFrames.push(...batchResults);
      } catch (error) {
        console.error(`\n❌ Error processing batch: ${error.message}`);
        throw error;
      }
    }

    console.log(`\n✅ Processed ${processedFrames.length} frames`);
    return processedFrames;
  }

  async combineFrames(outputVideo, fps = 30) {
    console.log('\n🎬 Step 3: Combining processed frames into video...');
    
    return new Promise((resolve, reject) => {
      const ffmpeg = spawn('ffmpeg', [
        '-framerate', fps.toString(),
        '-i', path.join(this.outputFolder, 'frame_%05d.jpg'),
        '-c:v', 'libx264',
        '-pix_fmt', 'yuv420p',
        '-crf', '18', // High quality
        '-y', // Overwrite output file
        outputVideo
      ]);

      ffmpeg.stderr.on('data', (data) => {
        if (data.toString().includes('frame=')) {
          process.stdout.write('.');
        }
      });

      ffmpeg.on('close', (code) => {
        if (code === 0) {
          const stats = fs.statSync(outputVideo);
          const fileSizeInMB = (stats.size / (1024 * 1024)).toFixed(2);
          console.log(`\n✅ Created: ${outputVideo} (${fileSizeInMB} MB)`);
          resolve(outputVideo);
        } else {
          reject(new Error(`FFmpeg failed with code ${code}`));
        }
      });
    });
  }

  async cleanup() {
    console.log('🧹 Cleaning up temporary files...');
    if (fs.existsSync(this.framesFolder)) {
      fs.rmSync(this.framesFolder, { recursive: true, force: true });
    }
    console.log('✅ Cleanup complete');
  }

  async processVideo(inputVideo, outputVideo, fps = 30, keepTempFrames = false) {
    console.log(`🚀 Starting video object detection pipeline...`);
    console.log(`📋 Input: ${inputVideo}`);
    console.log(`📋 Output: ${outputVideo}`);
    console.log(`📋 FPS: ${fps}`);
    
    try {
      // Step 1: Extract frames
      await this.extractFrames(inputVideo);
      
      // Step 2: Process frames with object detection
      await this.processAllFrames();
      
      // Step 3: Combine frames back to video
      await this.combineFrames(outputVideo, fps);
      
      // Step 4: Cleanup (optional)
      if (!keepTempFrames) {
        await this.cleanup();
      }
      
      console.log('\n🎉 Video processing complete!');
      console.log(`📄 Processed video saved as: ${outputVideo}`);
      if (keepTempFrames) {
        console.log(`📁 Processed frames saved in: ${this.outputFolder}`);
      }
      
    } catch (error) {
      console.error(`\n❌ Error: ${error.message}`);
      throw error;
    }
  }
}

// Usage
const inputVideo = process.argv[2];
const outputVideo = process.argv[3] || 'output_with_detection.mp4';
const fps = parseFloat(process.argv[4]) || 30;
const keepFrames = process.argv[5] === 'keep';

if (!inputVideo) {
  console.log('Usage: node video-object-detection.js <input_video> [output_video] [fps] [keep]');
  console.log('');
  console.log('Parameters:');
  console.log('  input_video  - Input video file path');
  console.log('  output_video - Output video file path (default: output_with_detection.mp4)');
  console.log('  fps         - Frame rate for output video (default: 30)');
  console.log('  keep        - Keep processed frames folder (default: cleanup)');
  console.log('');
  console.log('Examples:');
  console.log('  node video-object-detection.js input.mp4');
  console.log('  node video-object-detection.js input.mp4 detected.mp4 24');
  console.log('  node video-object-detection.js input.mp4 detected.mp4 30 keep');
  console.log('');
  console.log('⚠️  Make sure the object detection server is running!');
  console.log('   Start it with: node object-detection.js');
  process.exit(1);
}

// Check if object detection server is running
const testClient = net.createConnection(SOCKET_PATH);
testClient.on('connect', () => {
  testClient.destroy();
  
  // Server is running, start processing
  const processor = new VideoObjectDetection();
  processor.processVideo(inputVideo, outputVideo, fps, keepFrames)
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
