const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

async function extractFrames(videoPath, outputFolder, fps = 1) {
  // Check if video file exists
  if (!fs.existsSync(videoPath)) {
    throw new Error(`Video file not found: ${videoPath}`);
  }

  // Create output folder if it doesn't exist
  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  return new Promise((resolve, reject) => {
    console.log(`Extracting frames from ${videoPath} at ${fps} fps...`);
    
    const ffmpeg = spawn('ffmpeg', [
      '-i', videoPath,
      '-vf', `fps=${fps}`,
      '-y', // Overwrite output files
      '-q:v', '2', // High quality
      path.join(outputFolder, 'frame_%05d.jpg')
    ]);

    let stderr = '';

    ffmpeg.stderr.on('data', (data) => {
      stderr += data.toString();
      // Show progress
      if (data.toString().includes('frame=')) {
        process.stdout.write('.');
      }
    });

    ffmpeg.on('close', (code) => {
      if (code === 0) {
        console.log('\nFrame extraction complete!');
        
        // Count extracted frames
        const files = fs.readdirSync(outputFolder).filter(f => f.startsWith('frame_'));
        console.log(`Extracted ${files.length} frames`);
        resolve(files.length);
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

// Usage with better error handling
const videoPath = process.argv[2] || 'input.mp4';
const outputFolder = process.argv[3] || 'frames';
const fps = parseFloat(process.argv[4]) || 1;

// Show usage if no video file provided
if (!process.argv[2]) {
  console.log('Usage: node ffmpeg.js <video_file> [output_folder] [fps]');
  console.log('Example: node ffmpeg.js video.mp4 frames 1');
  process.exit(1);
}

extractFrames(videoPath, outputFolder, fps).catch((error) => {
  console.error('Error:', error.message);
  process.exit(1);
});