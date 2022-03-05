# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest

# Update apps on the base image
RUN apt-get -y update && apt-get install -y

# Install the Clang compiler
RUN apt-get -y install clang build-essential make

# Install C++ dependencies for SKO-Server
RUN apt-get -y install libmysql++-dev libargon2-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . .

# Specify the working directory
WORKDIR /

RUN make

LABEL Name=skoserver Version=1.4.0

# run sko server
CMD ["./skoserver-dev"]
