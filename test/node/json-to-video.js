const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

/**
 * Reconstructs a video from JSON frame objects
 * @param {string} jsonFolder - Path to folder containing JSON frame files
 * @param {string} outputVideoPath - Path for the output video file
 * @param {object} options - Video encoding options
 * @returns {Promise<string>} - Promise that resolves with output video path
 */
async function reconstructVideoFromJSON(jsonFolder, outputVideoPath, options = {}) {
  // Default options
  const {
    framerate = 25,
    videoCodec = 'libx264',
    quality = 'high', // 'low', 'medium', 'high'
    format = 'mp4',
    resolution = null, // Auto-detect from first frame
    deleteTemp = true
  } = options;

  console.log(`🎬 Starting video reconstruction from JSON frames...`);
  console.log(`📁 Input folder: ${jsonFolder}`);
  console.log(`🎥 Output video: ${outputVideoPath}`);

  // Check if JSON folder exists
  if (!fs.existsSync(jsonFolder)) {
    throw new Error(`JSON folder not found: ${jsonFolder}`);
  }

  // Create output directory if it doesn't exist
  const outputDir = path.dirname(outputVideoPath);
  if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true });
  }

  // Get all JSON frame files
  const jsonFiles = fs.readdirSync(jsonFolder)
    .filter(f => f.endsWith('.json') && (f.startsWith('frame_') || f.includes('frame')))
    .sort((a, b) => {
      // Extract frame numbers for proper sorting
      const aNum = extractFrameNumber(a);
      const bNum = extractFrameNumber(b);
      return aNum - bNum;
    });

  if (jsonFiles.length === 0) {
    throw new Error(`No JSON frame files found in: ${jsonFolder}`);
  }

  console.log(`📊 Found ${jsonFiles.length} JSON frame files`);

  // Create temporary folder for extracted images
  const tempImageFolder = path.join(jsonFolder, 'temp_reconstruction');
  if (fs.existsSync(tempImageFolder)) {
    fs.rmSync(tempImageFolder, { recursive: true, force: true });
  }
  fs.mkdirSync(tempImageFolder, { recursive: true });

  console.log(`🖼️  Converting JSON frames to images...`);

  try {
    // Convert JSON frames to image files
    let frameCount = 0;
    for (const jsonFile of jsonFiles) {
      const jsonPath = path.join(jsonFolder, jsonFile);
      
      try {
        // Read and parse JSON file
        const jsonData = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
        
        // Validate JSON structure
        if (!jsonData.image_data && !jsonData.imageData) {
          console.warn(`⚠️  Skipping ${jsonFile}: No image data found`);
          continue;
        }

        // Get base64 image data (handle both naming conventions)
        const imageBase64 = jsonData.image_data || jsonData.annotated_image;
        const imageBuffer = Buffer.from(imageBase64, 'base64');
        
        // Determine output filename with proper numbering
        const frameNumber = jsonData.frameNumber || (frameCount + 1);
        const frameFileName = `frame_${frameNumber.toString().padStart(5, '0')}.jpg`;
        const imagePath = path.join(tempImageFolder, frameFileName);
        
        // Write image file
        fs.writeFileSync(imagePath, imageBuffer);
        frameCount++;
        
        // Show progress
        if (frameCount % 50 === 0 || frameCount === jsonFiles.length) {
          process.stdout.write(`\r🔄 Converted ${frameCount}/${jsonFiles.length} frames`);
        }
        
      } catch (error) {
        console.warn(`\n⚠️  Error processing ${jsonFile}: ${error.message}`);
      }
    }

    if (frameCount === 0) {
      throw new Error('No valid frames were extracted from JSON files');
    }

    console.log(`\n✅ Successfully converted ${frameCount} frames to images`);

    // Set video quality parameters
    let qualityParams = [];
    switch (quality) {
      case 'low':
        qualityParams = ['-crf', '30', '-preset', 'fast'];
        break;
      case 'medium':
        qualityParams = ['-crf', '23', '-preset', 'medium'];
        break;
      case 'high':
      default:
        qualityParams = ['-crf', '18', '-preset', 'slow'];
        break;
    }

    // Build FFmpeg command
    const ffmpegArgs = [
      '-y', // Overwrite output file
      '-framerate', framerate.toString(),
      '-i', path.join(tempImageFolder, 'frame_%05d.jpg'),
      '-c:v', videoCodec,
      ...qualityParams,
      '-pix_fmt', 'yuv420p', // Ensure compatibility
    ];

    // Add resolution if specified
    if (resolution) {
      ffmpegArgs.push('-s', resolution);
    }

    // Add output path
    ffmpegArgs.push(outputVideoPath);

    console.log(`🎬 Creating video with FFmpeg...`);
    console.log(`⚙️  Parameters: ${framerate}fps, ${quality} quality, ${videoCodec} codec`);

    // Execute FFmpeg
    await new Promise((resolve, reject) => {
      const ffmpeg = spawn('ffmpeg', ffmpegArgs);

      let stderr = '';
      let lastProgress = '';

      ffmpeg.stderr.on('data', (data) => {
        stderr += data.toString();
        
        // Extract progress information
        const progressMatch = data.toString().match(/frame=\s*(\d+)/);
        if (progressMatch) {
          const currentFrame = parseInt(progressMatch[1]);
          const progress = `🎬 Encoding: ${currentFrame}/${frameCount} frames (${Math.round(currentFrame/frameCount*100)}%)`;
          if (progress !== lastProgress) {
            process.stdout.write(`\r${progress}`);
            lastProgress = progress;
          }
        }
      });

      ffmpeg.on('close', (code) => {
        if (code === 0) {
          console.log('\n✅ Video reconstruction complete!');
          resolve();
        } else {
          console.error('\n❌ FFmpeg stderr:', stderr);
          reject(new Error(`FFmpeg process exited with code ${code}`));
        }
      });

      ffmpeg.on('error', (err) => {
        reject(new Error(`Failed to start FFmpeg: ${err.message}`));
      });
    });

    // Get output video stats
    const videoStats = fs.statSync(outputVideoPath);
    console.log(`📊 Output video: ${(videoStats.size / 1024 / 1024).toFixed(2)} MB`);
    console.log(`🎯 Video saved: ${outputVideoPath}`);

    return outputVideoPath;

  } finally {
    // Clean up temporary files
    if (deleteTemp && fs.existsSync(tempImageFolder)) {
      console.log('🧹 Cleaning up temporary files...');
      fs.rmSync(tempImageFolder, { recursive: true, force: true });
    }
  }
}

/**
 * Extract frame number from filename
 */
function extractFrameNumber(filename) {
  const match = filename.match(/(\d+)/);
  return match ? parseInt(match[1]) : 0;
}

/**
 * Get video information from JSON frames
 */
function getVideoInfoFromJSON(jsonFolder) {
  const jsonFiles = fs.readdirSync(jsonFolder)
    .filter(f => f.endsWith('.json') && (f.startsWith('frame_') || f.includes('frame')))
    .sort();

  if (jsonFiles.length === 0) {
    throw new Error(`No JSON frame files found in: ${jsonFolder}`);
  }

  // Read first JSON file to get frame info
  const firstJsonPath = path.join(jsonFolder, jsonFiles[0]);
  const firstFrame = JSON.parse(fs.readFileSync(firstJsonPath, 'utf8'));

  // Calculate total duration (assuming 25fps default)
  const totalFrames = jsonFiles.length;
  const estimatedDuration = totalFrames / 25; // seconds

  return {
    totalFrames,
    estimatedDuration,
    firstFrameSize: firstFrame.size || 0,
    format: firstFrame.format || 'jpg',
    sampleJson: firstFrame
  };
}

/**
 * Batch process multiple JSON folders into videos
 */
async function batchReconstructVideos(inputFolders, outputFolder, options = {}) {
  console.log(`🎬 Starting batch video reconstruction...`);
  
  if (!fs.existsSync(outputFolder)) {
    fs.mkdirSync(outputFolder, { recursive: true });
  }

  const results = [];
  
  for (let i = 0; i < inputFolders.length; i++) {
    const inputFolder = inputFolders[i];
    const folderName = path.basename(inputFolder);
    const outputVideoPath = path.join(outputFolder, `${folderName}.mp4`);
    
    console.log(`\n📁 Processing folder ${i + 1}/${inputFolders.length}: ${folderName}`);
    
    try {
      const videoPath = await reconstructVideoFromJSON(inputFolder, outputVideoPath, options);
      results.push({ success: true, input: inputFolder, output: videoPath });
      console.log(`✅ Completed: ${folderName}`);
    } catch (error) {
      console.error(`❌ Failed: ${folderName} - ${error.message}`);
      results.push({ success: false, input: inputFolder, error: error.message });
    }
  }
  
  console.log(`\n🎯 Batch processing complete: ${results.filter(r => r.success).length}/${results.length} successful`);
  return results;
}

// Command line interface
if (require.main === module) {
  const command = process.argv[2];
  const inputPath = process.argv[3];
  const outputPath = process.argv[4];

  // Show usage if no command provided
  if (!command) {
    console.log('🎬 JSON to Video Reconstruction Tool');
    console.log('');
    console.log('Usage:');
    console.log('  Reconstruct single video:');
    console.log('    node json-to-video.js reconstruct <json_folder> <output_video.mp4> [options]');
    console.log('');
    console.log('  Get video info:');
    console.log('    node json-to-video.js info <json_folder>');
    console.log('');
    console.log('  Batch reconstruct:');
    console.log('    node json-to-video.js batch <input_folders_list> <output_folder>');
    console.log('');
    console.log('Options (for reconstruct command):');
    console.log('  --framerate <fps>     Video framerate (default: 25)');
    console.log('  --quality <level>     Video quality: low/medium/high (default: high)');
    console.log('  --codec <codec>       Video codec (default: libx264)');
    console.log('  --resolution <size>   Force resolution (e.g., 1920x1080)');
    console.log('  --keep-temp          Keep temporary image files');
    console.log('');
    console.log('Examples:');
    console.log('  node json-to-video.js reconstruct json_frames output.mp4');
    console.log('  node json-to-video.js reconstruct json_frames output.mp4 --framerate 30 --quality high');
    console.log('  node json-to-video.js info json_frames');
    console.log('');
    process.exit(1);
  }

  // Parse command line options
  const options = {};
  const args = process.argv.slice(5);
  for (let i = 0; i < args.length; i += 2) {
    const flag = args[i];
    const value = args[i + 1];
    
    switch (flag) {
      case '--framerate':
        options.framerate = parseInt(value);
        break;
      case '--quality':
        options.quality = value;
        break;
      case '--codec':
        options.videoCodec = value;
        break;
      case '--resolution':
        options.resolution = value;
        break;
      case '--keep-temp':
        options.deleteTemp = false;
        i--; // No value for this flag
        break;
    }
  }

  // Execute commands
  if (command === 'reconstruct') {
    if (!inputPath || !outputPath) {
      console.error('❌ Error: JSON folder and output video path are required');
      process.exit(1);
    }
    
    reconstructVideoFromJSON(inputPath, outputPath, options).catch((error) => {
      console.error('❌ Error:', error.message);
      process.exit(1);
    });
    
  } else if (command === 'info') {
    if (!inputPath) {
      console.error('❌ Error: JSON folder path is required');
      process.exit(1);
    }
    
    try {
      const info = getVideoInfoFromJSON(inputPath);
      console.log('📊 Video Information:');
      console.log(`   Total frames: ${info.totalFrames}`);
      console.log(`   Estimated duration: ${info.estimatedDuration.toFixed(2)} seconds`);
      console.log(`   Frame format: ${info.format}`);
      console.log(`   Sample frame size: ${(info.firstFrameSize / 1024).toFixed(2)} KB`);
    } catch (error) {
      console.error('❌ Error:', error.message);
      process.exit(1);
    }
    
  } else if (command === 'batch') {
    if (!inputPath || !outputPath) {
      console.error('❌ Error: Input folders list and output folder are required');
      process.exit(1);
    }
    
    // Read folders from file or use glob pattern
    let inputFolders = [];
    if (fs.existsSync(inputPath) && fs.statSync(inputPath).isFile()) {
      // Read from file
      inputFolders = fs.readFileSync(inputPath, 'utf8').split('\n').filter(line => line.trim());
    } else {
      // Single folder
      inputFolders = [inputPath];
    }
    
    batchReconstructVideos(inputFolders, outputPath, options).catch((error) => {
      console.error('❌ Error:', error.message);
      process.exit(1);
    });
    
  } else {
    console.error('❌ Error: Unknown command. Use "reconstruct", "info", or "batch"');
    process.exit(1);
  }
}

// Export functions for use in other modules
module.exports = {
  reconstructVideoFromJSON,
  getVideoInfoFromJSON,
  batchReconstructVideos
};
