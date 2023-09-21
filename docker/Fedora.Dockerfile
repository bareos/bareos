FROM fedora:latest
MAINTAINER Peter Spiess-Knafl <dev@spiessknafl.at>
ENV OS=fedora
RUN mkdir /app
COPY docker/deps-fedora.sh /app
RUN chmod a+x /app/deps-fedora.sh
RUN /app/deps-fedora.sh
COPY docker/build_test_install.sh /app
COPY . /app
RUN chmod a+x /app/build_test_install.sh
RUN cd /app && ./build_test_install.sh
