# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest as builder
WORKDIR /app

# Update apps on the base image 
RUN DEBIAN_FRONTEND=noninteractive && apt-get -yq update && apt-get install -yq 

# Install the Clang compiler
RUN DEBIAN_FRONTEND=noninteractive && apt-get -yq install clang build-essential

# Install C++ dependencies for SKO-Server
RUN DEBIAN_FRONTEND=noninteractive && apt-get -yq install libmysql++-dev libargon2-dev libcrypto++-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . .
RUN make

FROM ubuntu:latest
WORKDIR /root

RUN DEBIAN_FRONTEND=noninteractive && apt-get -yq update && DEBIAN_FRONTEND=noninteractive && apt-get install -y 
RUN DEBIAN_FRONTEND=noninteractive && apt-get -yq install libmysql++-dev libargon2-dev libcrypto++-dev

COPY --from=builder /app/skoserver-dev .
COPY --from=builder /app/SKO_Content/* SKO_Content/

EXPOSE 1337/TCP
# run sko server
CMD ["./skoserver-dev"]