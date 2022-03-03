# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest

# Update apps on the base image
RUN apt -y update && apt-get install -y

# Install the Clang compiler
RUN apt -y install clang install build-essential install make

# Install C++ dependencies for SKO-Server
RUN apt -y install libmysql++-dev libargon2-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . .

# Specify the working directory
WORKDIR /

RUN make

LABEL Name=skoserver Version=1.4.0
