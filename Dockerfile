FROM ubuntu:bionic

RUN apt-get update && \
    apt-get install -y \
        cmake \
	build-essential \
	gcc \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        git \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        libegl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        freeglut3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        libegl-mesa0 \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        nouveau-firmware \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        mesa-utils \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/libmicrohttpd
COPY libmicrohttpd-0.9.70 /opt/libmicrohttpd
RUN ./configure && \
    make && \
    make install

WORKDIR /app
COPY . /app
RUN cmake -S . -Bbuild && \
    make -C build

CMD ["/app/build/render"]
