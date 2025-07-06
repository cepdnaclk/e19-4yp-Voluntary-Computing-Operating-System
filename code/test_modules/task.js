const { createCanvas } = require('canvas');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const ffmpegPath = require('ffmpeg-static');

const frameCount = 30;
const width = 320;
const height = 240;
const outputDir = path.join(__dirname, 'frames');

// Create frames directory if it doesn't exist
if (!fs.existsSync(outputDir)) {
  fs.mkdirSync(outputDir);
}

// Step 1: Generate PNG frames using canvas
for (let i = 0; i < frameCount; i++) {
  const canvas = createCanvas(width, height);
  const ctx = canvas.getContext('2d');

  // Fill background with a gradient color
  ctx.fillStyle = `rgb(${i * 8}, ${i * 4}, ${255 - i * 8})`;
  ctx.fillRect(0, 0, width, height);

  // Draw a moving circle
  ctx.beginPath();
  ctx.arc(50 + i * 5, height / 2, 30, 0, Math.PI * 2);
  ctx.fillStyle = 'white';
  ctx.fill();

  const buffer = canvas.toBuffer('image/png');
  const framePath = path.join(outputDir, `frame_${String(i).padStart(3, '0')}.png`);
  fs.writeFileSync(framePath, buffer);
}

console.log('[TASK] Frames generated. Now rendering video...');

// Step 2: Use FFmpeg to convert frames to video
const ffmpegArgs = [
  '-framerate', '10',
  '-i', path.join(outputDir, 'frame_%03d.png'),
  '-c:v', 'libx264',
  '-pix_fmt', 'yuv420p',
  'output.mp4'
];

const ffmpeg = spawn(ffmpegPath, ffmpegArgs);

ffmpeg.stdout.on('data', (data) => {
  console.log(`stdout: ${data}`);
});

ffmpeg.stderr.on('data', (data) => {
  console.error(`stderr: ${data}`);
});

ffmpeg.on('close', (code) => {
  if (code === 0) {
    console.log('[TASK] Video rendering complete: output.mp4');
    // Optionally clean up frames
    fs.rmSync(outputDir, { recursive: true, force: true });
  } else {
    console.error(`[TASK] FFmpeg process exited with code ${code}`);
  }
});
