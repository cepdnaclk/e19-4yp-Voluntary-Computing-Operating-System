const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

async function extractFrames(videoPath, outputFolder, extractAll = true, fps = null) {
  // Check if video file exists
  if (!fs.existsSync(videoPath)) {
    throw new Error(`Video file not found: ${videoPath}`);
  }

  // Create output folder if it doesn't exist
  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  return new Promise((resolve, reject) => {
    let ffmpegArgs;
    
    if (extractAll) {
      console.log(`Extracting ALL frames from ${videoPath} at original frame rate...`);
      // Extract all frames without frame rate filtering
      ffmpegArgs = [
        '-i', videoPath,
        '-y', // Overwrite output files
        '-q:v', '2', // High quality
        path.join(outputFolder, 'frame_%05d.jpg')
      ];
    } else {
      const targetFps = fps || 1;
      console.log(`Extracting frames from ${videoPath} at ${targetFps} fps...`);
      // Extract at specified frame rate
      ffmpegArgs = [
        '-i', videoPath,
        '-vf', `fps=${targetFps}`,
        '-y', // Overwrite output files
        '-q:v', '2', // High quality
        path.join(outputFolder, 'frame_%05d.jpg')
      ];
    }
    
    const ffmpeg = spawn('ffmpeg', ffmpegArgs);

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
const mode = process.argv[4] || 'all'; // 'all' or 'fps'
const fps = parseFloat(process.argv[5]) || 1;

// Show usage if no video file provided
if (!process.argv[2]) {
  console.log('Usage: node ffmpeg.js <video_file> [output_folder] [mode] [fps]');
  console.log('Modes:');
  console.log('  all  - Extract ALL frames at original frame rate (default)');
  console.log('  fps  - Extract frames at specified fps rate');
  console.log('');
  console.log('Examples:');
  console.log('  node ffmpeg.js video.mp4                    # Extract all frames');
  console.log('  node ffmpeg.js video.mp4 frames all         # Extract all frames to "frames" folder');
  console.log('  node ffmpeg.js video.mp4 frames fps 1       # Extract 1 frame per second');
  console.log('  node ffmpeg.js video.mp4 frames fps 0.5     # Extract 1 frame every 2 seconds');
  process.exit(1);
}

const extractAll = mode === 'all';
extractFrames(videoPath, outputFolder, extractAll, fps).catch((error) => {
  console.error('Error:', error.message);
  process.exit(1);
});