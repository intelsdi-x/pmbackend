FROM centos
RUN curl -O https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN rpm -ivh epel-release-latest-7.noarch.rpm
RUN yum install -y cmake make gtest-devel gcc gcc-c++ libuuid-devel pkgconfig
ADD . /tmp
RUN cd /tmp && mkdir build && cd build && cmake .. && make && ./tests_runner
