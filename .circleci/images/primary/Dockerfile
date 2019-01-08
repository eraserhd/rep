FROM oracle/graalvm-ce:1.0.0-rc10

RUN yum install -y git ssh ca-certificates patch asciidoctor

RUN curl -L http://mirror.cc.columbia.edu/pub/software/apache/maven/maven-3/3.6.0/binaries/apache-maven-3.6.0-bin.tar.gz |tar xzf - && \
    ln -sf $PWD/apache-maven-*/bin/mvn /usr/bin/mvn

ADD build.sh ./
RUN ./build.sh
