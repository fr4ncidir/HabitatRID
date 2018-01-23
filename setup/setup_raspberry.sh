#! /bin/bash

cd
mkdir projects
cd projects
git clone https://github.com/vaimee/sepa-C-kpi.git
git clone https://github.com/warmcat/libwebsockets.git
git clone https://github.com/fr4ncidir/SerialLib.git
sudo apt-get install figlet cmake libssl-dev libcurl4-gnutls-dev libglib2.0-dev python-pip libgsl0-dev socat
cd ./libwebsockets
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
cd ../../SerialLib
eval `cat ridMain.c | grep gcc`
cd
printf "\n"
pwd
