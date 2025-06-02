// function heavyTask() {
//     let sum = 0;
//     for (let i = 0; i < 1e7; i++) sum += i;
//     return sum;
// }
// console.log(heavyTask());

// //rendering
// const { createCanvas } = require('canvas');
// const fs = require('fs');

// // Create a 400x200 canvas
// const width = 400;
// const height = 200;
// const canvas = createCanvas(width, height);
// const ctx = canvas.getContext('2d');

// // Background
// ctx.fillStyle = '#222';
// ctx.fillRect(0, 0, width, height);

// // Draw a red rectangle
// ctx.fillStyle = 'red';
// ctx.fillRect(50, 50, 100, 100);

// // Draw text
// ctx.fillStyle = 'white';
// ctx.font = 'bold 20px Sans';
// ctx.fillText('Hello from Worker!', 160, 120);

// // Save to file
// const out = fs.createWriteStream('output.png');
// const stream = canvas.createPNGStream();
// stream.pipe(out);

// out.on('finish', () => {
//     console.log('Rendering complete. Image saved as output.png');
// });

//complex rendering
const { createCanvas } = require('canvas');
const fs = require('fs');

// Canvas setup
const width = 800;
const height = 400;
const canvas = createCanvas(width, height);
const ctx = canvas.getContext('2d');

// Background
ctx.fillStyle = '#111';
ctx.fillRect(0, 0, width, height);

// Axes
ctx.strokeStyle = '#aaa';
ctx.lineWidth = 1;

// X-axis
ctx.beginPath();
ctx.moveTo(40, height / 2);
ctx.lineTo(width - 20, height / 2);
ctx.stroke();

// Y-axis
ctx.beginPath();
ctx.moveTo(40, 20);
ctx.lineTo(40, height - 20);
ctx.stroke();

// Function to plot sine waves
function drawSineWave(frequency, amplitude, color, phaseShift = 0) {
    ctx.beginPath();
    ctx.lineWidth = 2;
    ctx.strokeStyle = color;

    for (let x = 0; x <= width - 60; x++) {
        const px = x + 40;
        const py = height / 2 - amplitude * Math.sin((x / width) * 4 * Math.PI * frequency + phaseShift);
        if (x === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
    }

    ctx.stroke();
}

// Plot multiple waves
drawSineWave(1, 40, 'red');
drawSineWave(2, 30, 'green', Math.PI / 4);
drawSineWave(3, 20, 'blue', Math.PI / 2);

// Legend
ctx.fillStyle = 'white';
ctx.font = '14px Sans';
ctx.fillText('Sine Wave 1Hz', 650, 40);
ctx.fillStyle = 'red';
ctx.fillRect(630, 32, 12, 12);

ctx.fillStyle = 'white';
ctx.fillText('Sine Wave 2Hz', 650, 60);
ctx.fillStyle = 'green';
ctx.fillRect(630, 52, 12, 12);

ctx.fillStyle = 'white';
ctx.fillText('Sine Wave 3Hz', 650, 80);
ctx.fillStyle = 'blue';
ctx.fillRect(630, 72, 12, 12);

// Save image
const out = fs.createWriteStream('sine_wave_plot.png');
const stream = canvas.createPNGStream();
stream.pipe(out);

out.on('finish', () => {
    console.log('Rendering complete. Image saved as sine_wave_plot.png');
});
