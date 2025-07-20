const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

async function combineFrames(framesFolder, outputVideo, fps = 30, quality = 'high') {
  // Check if frames folder exists
  if (!fs.existsSync(framesFolder)) {
    throw new Error(`Frames folder not found: ${framesFolder}`);
  }

  // Count frame files
  const frameFiles = fs.readdirSync(framesFolder)
    .filter(f => f.match(/frame_\d+\.jpg$/))
    .sort();
  
  if (frameFiles.length === 0) {
    throw new Error(`No frame files found in ${framesFolder}`);
  }

  console.log(`Found ${frameFiles.length} frames to combine`);

  return new Promise((resolve, reject) => {
    console.log(`Combining frames from ${framesFolder} into ${outputVideo} at ${fps} fps...`);
    
    // Set quality parameters
    let qualityArgs;
    switch (quality.toLowerCase()) {
      case 'high':
        qualityArgs = ['-crf', '18']; // High quality
        break;
      case 'medium':
        qualityArgs = ['-crf', '23']; // Medium quality
        break;
      case 'low':
        qualityArgs = ['-crf', '28']; // Lower quality, smaller file
        break;
      default:
        qualityArgs = ['-crf', '18']; // Default to high quality
    }
    
    const ffmpegArgs = [
      '-framerate', fps.toString(),
      '-i', path.join(framesFolder, 'frame_%05d.jpg'),
      '-c:v', 'libx264',
      '-pix_fmt', 'yuv420p',
      ...qualityArgs,
      '-y', // Overwrite output file
      outputVideo
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

    ffmpeg.on('close', (code) => {
      if (code === 0) {
        console.log('\nVideo creation complete!');
        
        // Get output file size
        const stats = fs.statSync(outputVideo);
        const fileSizeInMB = (stats.size / (1024 * 1024)).toFixed(2);
        console.log(`Created: ${outputVideo} (${fileSizeInMB} MB)`);
        resolve(outputVideo);
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

// Usage
const framesFolder = process.argv[2] || 'detected_frames';
const outputVideo = process.argv[3] || 'output_processed.mp4';
const fps = parseFloat(process.argv[4]) || 30;
const quality = process.argv[5] || 'high';

// Show usage if no frames folder provided
if (!process.argv[2]) {
  console.log('Usage: node combine-frames.js <frames_folder> [output_video] [fps] [quality]');
  console.log('');
  console.log('Parameters:');
  console.log('  frames_folder - Folder containing frame_*.jpg files');
  console.log('  output_video  - Output video filename (default: output_processed.mp4)');
  console.log('  fps          - Frame rate for output video (default: 30)');
  console.log('  quality      - Video quality: high, medium, low (default: high)');
  console.log('');
  console.log('Examples:');
  console.log('  node combine-frames.js detected_frames                           # Use defaults');
  console.log('  node combine-frames.js detected_frames output.mp4 30 high       # High quality 30fps');
  console.log('  node combine-frames.js detected_frames compressed.mp4 24 medium # Medium quality 24fps');
  process.exit(1);
}

combineFrames(framesFolder, outputVideo, fps, quality).catch((error) => {
  console.error('Error:', error.message);
  process.exit(1);
});
