FROM ubuntu:18.04

ENV user cppdev

RUN sed -i.bak -e "s%http://[^ ]\+%http://ftp.jaist.ac.jp/pub/Linux/ubuntu/%g" /etc/apt/sources.list
RUN apt-get update && \
    apt-get install -y git build-essential libboost-all-dev \
    cmake clang libssl-dev hwloc valgrind vim sudo

# add user
RUN useradd -d /home/$user -m -s /bin/bash $user
RUN echo "$user:$user" | chpasswd
RUN echo "$user ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$user
USER $user
ENV HOME /home/$user

# Work Directory
# RUN sudo mkdir -p $deploy
# RUN sudo chown -R $user:$user $deploy
WORKDIR $HOME