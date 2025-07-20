const path = require('path');
const os = require('os');

// Explicitly add the global modules path
const globalModulesPath = path.join(os.homedir(), 'node_global_modules', 'node_modules');
module.paths.unshift(globalModulesPath);

// object-detection.js
const tf = require('@tensorflow/tfjs-node');
const cocoSsd = require('@tensorflow-models/coco-ssd');
const fs = require('fs');
const { createCanvas, loadImage } = require('canvas');
// const path = require('path');

async function detectObjectsInFrames(inputFolder, outputFolder) {
  // Create output folder if it doesn't exist
  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  // Load the COCO-SSD model
  const model = await cocoSsd.load();

  // Get all image files from input folder
  const files = fs.readdirSync(inputFolder).filter(file => 
    ['.jpg', '.jpeg', '.png'].includes(path.extname(file).toLowerCase())
  );

  console.log(`Processing ${files.length} images...`);

  for (const file of files) {
    const inputPath = path.join(inputFolder, file);
    const outputPath = path.join(outputFolder, file);

    // Load the image
    const image = await loadImage(inputPath);
    const canvas = createCanvas(image.width, image.height);
    const ctx = canvas.getContext('2d');
    ctx.drawImage(image, 0, 0);

    // Perform object detection
    const predictions = await model.detect(canvas);

    // Draw bounding boxes
    ctx.strokeStyle = '#00FF00';
    ctx.lineWidth = 4;
    ctx.font = '20px Arial';
    ctx.fillStyle = '#00FF00';

    for (const prediction of predictions) {
      // Draw rectangle
      ctx.beginPath();
      ctx.rect(
        prediction.bbox[0],
        prediction.bbox[1],
        prediction.bbox[2],
        prediction.bbox[3]
      );
      ctx.stroke();

      // Draw label background
      ctx.fillStyle = '#00FF00';
      const textWidth = ctx.measureText(`${prediction.class} (${Math.round(prediction.score * 100)}%)`).width;
      ctx.fillRect(
        prediction.bbox[0],
        prediction.bbox[1] - 30,
        textWidth + 10,
        30
      );

      // Draw text
      ctx.fillStyle = '#000000';
      ctx.fillText(
        `${prediction.class} (${Math.round(prediction.score * 100)}%)`,
        prediction.bbox[0] + 5,
        prediction.bbox[1] - 10
      );
    }

    // Save the image with bounding boxes
    const out = fs.createWriteStream(outputPath);
    const stream = canvas.createJPEGStream();
    stream.pipe(out);

    console.log(`Processed ${file} - detected ${predictions.length} objects`);
  }

  console.log('Object detection complete!');
}

// Usage example
const inputFolder = 'frames'; // Folder with extracted frames
const outputFolder = 'detected_frames';
detectObjectsInFrames(inputFolder, outputFolder).catch(console.error);
