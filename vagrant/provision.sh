sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y make gcc libc6:i386 libncurses5:i386 libstdc++6:i386 libc6-i386 lib32z1 sshpass libncurses5-dev libncursesw5-dev

echo "cd /repos/unum-v2" >> /home/ubuntu/.bashrc
