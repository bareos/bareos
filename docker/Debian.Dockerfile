FROM debian:latest
MAINTAINER Peter Spiess-Knafl <dev@spiessknafl.at>
ENV OS=debian
RUN mkdir /app
COPY docker/deps-debian.sh /app
RUN chmod a+x /app/deps-debian.sh
RUN /app/deps-debian.sh
COPY docker/build_test_install.sh /app
COPY . /app
RUN chmod a+x /app/build_test_install.sh
RUN cd /app && ./build_test_install.sh
