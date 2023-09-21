FROM archlinux:latest
MAINTAINER Peter Spiess-Knafl <dev@spiessknafl.at>
ENV OS=archlinux
RUN mkdir /app
COPY docker/deps-archlinux.sh /app
RUN chmod a+x /app/deps-archlinux.sh
RUN /app/deps-archlinux.sh
COPY docker/build_test_install.sh /app
COPY . /app
RUN chmod a+x /app/build_test_install.sh
RUN cd /app && ./build_test_install.sh
