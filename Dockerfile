# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest as builder
WORKDIR /app

# Update apps on the base image 
RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive && apt-get install -y 

# Install the Clang compiler
RUN apt-get -y install clang build-essential

# Install C++ dependencies for SKO-Server
RUN apt-get -y install libmysql++-dev libargon2-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . .
RUN make

FROM ubuntu:latest
WORKDIR /root

RUN apt-get -y update && DEBIAN_FRONTEND=noninteractive && apt-get install -y 
RUN apt-get -y install libmysql++-dev libargon2-dev

COPY --from=builder /app/skoserver-dev .
COPY --from=builder /app/SKO_Content/* SKO_Content/

# run sko server
CMD ["./skoserver-dev"]