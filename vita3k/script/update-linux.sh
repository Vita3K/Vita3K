#!/bin/sh
echo Download and extract the latest build of Vita3K in progress... && \
curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/ubuntu-latest.zip -o ./ubuntu-latest.zip && \
unzip -q -o ./ubuntu-latest.zip && \
rm ./ubuntu-latest.zip && \
echo Finished! && \
read -p "Press [Enter] key to continue..."
