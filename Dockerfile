# Original is from https://www.sigbus.info/compilerbook/Dockerfile
# Modified by mityu; added "--platform=linux/amd64" option
FROM --platform=linux/amd64 ubuntu:latest
RUN apt update
RUN DEBIAN_FRONTEND=noninteractive apt install -y gcc make git binutils libc6-dev gdb sudo
RUN adduser --disabled-password --gecos '' user
RUN echo 'user ALL=(root) NOPASSWD:ALL' > /etc/sudoers.d/user
USER user
WORKDIR /home/user
