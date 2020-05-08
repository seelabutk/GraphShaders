FROM ubuntu:latest

RUN apt-get update
RUN DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata
RUN apt-get install -y \
  python3 \
  python3-pip \
  vim \
  curl \
  wget \
  default-jdk \
  ntp

RUN pip3 install graphviz

RUN wget https://github.com/klarman-cell-observatory/forceatlas2/releases/download/1.0.3/gephi-toolkit-0.9.2-all.jar
RUN wget https://github.com/klarman-cell-observatory/forceatlas2/releases/download/1.0.3/forceatlas2.jar

RUN apt-get install -y \
  git \
  build-essential \
  libtool \
  automake \
  make \
  bison \
  flex \
  zlib1g-dev \
  libxml2-dev \
  python3-dev
RUN pip3 install python-igraph

RUN apt-get install -y \
  graphviz \
  libboost-all-dev \
  bgpdump

#RUN wget http://apropos.icmb.utexas.edu/lgl/files/LGL.tar.gz
#RUN tar -zxvf LGL.tar.gz
#RUN cd LGL-1.1 && .setup -i && ./setup -c conf_file && ./bin/lgl.pl -c conf_file
RUN echo 'Making LGLx2'
RUN git clone https://github.com/3PIV/LGL.git
WORKDIR /LGL
RUN ./setup.pl -c ./conf_file
RUN ./setup.pl -i  
RUN cp ./perls/*.pm /etc/perl
RUN cp ./bin/lglayout2D /usr/local/bin

RUN apt-get update && apt-get clean

