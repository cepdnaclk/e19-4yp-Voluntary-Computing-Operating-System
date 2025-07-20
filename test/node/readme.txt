mkdir ~/node_global_modules
cd ~/node_global_modules
sudo apt install ffmpeg
npm install canvas jsdom @tensorflow/tfjs-node @tensorflow-models/coco-ssd
export NODE_PATH=~/node_global_modules/node_modules
