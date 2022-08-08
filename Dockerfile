# Get the base Ubuntu image from Docker Hub
FROM ubuntu:latest as builder
WORKDIR /app

# Update apps on the base image
RUN apt-get -y update && apt-get install -y

# Install the Clang compiler
RUN apt-get -y install clang build-essential make

# Install C++ dependencies for SKO-Server
RUN apt-get -y install libmysql++-dev libargon2-dev

# Copy the current folder which contains C++ source code to the Docker image under /usr/src
COPY . .
RUN make

FROM ubuntu:latest
WORKDIR /root/
COPY --from=builder /app/bin/skoserver-dev /usr/local/bin/

EXPOSE 1337

# run sko server
CMD ["./skoserver-dev"]
