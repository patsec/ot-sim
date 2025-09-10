FROM debian:bookworm

ENV TZ=America/Denver
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt install -y \
  7zip bash-completion build-essential cmake curl git git-lfs mbpoll sudo tmux tree vim wget xz-utils \
  cmake libboost-dev libczmq-dev libxml2-dev libzmq3-dev pkg-config python3-dev python3-pip

ARG USERNAME=vscode
ARG USER_UID=1000

RUN useradd -l -u $USER_UID -md /home/$USERNAME -s /bin/bash -p $USERNAME $USERNAME \
  && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
  && chmod 0440 /etc/sudoers.d/$USERNAME

ADD https://raw.githubusercontent.com/git/git/master/contrib/completion/git-prompt.sh /home/$USERNAME/.bash/git-prompt.sh
RUN chown $USERNAME:$USERNAME /home/$USERNAME/.bash/git-prompt.sh \
  && chmod +x /home/$USERNAME/.bash/git-prompt.sh \
  && echo "\nsource ~/.bash/git-prompt.sh" >> /home/$USERNAME/.bashrc \
  && echo "export GIT_PS1_SHOWCOLORHINTS=true" >> /home/$USERNAME/.bashrc \
  && echo "export PROMPT_COMMAND='__git_ps1 \"\W\" \" » \"'" >> /home/$USERNAME/.bashrc

ARG GOLANG_VERSION=1.21.1

RUN wget -O go.tgz https://golang.org/dl/go${GOLANG_VERSION}.linux-amd64.tar.gz \
  && tar -C /usr/local -xzf go.tgz && rm go.tgz \
  && ln -s /usr/local/go/bin/* /usr/local/bin

ENV GOPATH /go
ENV PATH $GOPATH/bin:$PATH

RUN mkdir -p "$GOPATH/src" "$GOPATH/bin" \
  && chmod -R 777 "$GOPATH"

RUN go install github.com/ramya-rao-a/go-outline@latest 2>&1
RUN go install github.com/mdempsky/gocode@latest 2>&1
RUN go install github.com/stamblerre/gocode@latest 2>&1
RUN go install github.com/rogpeppe/godef@latest 2>&1
RUN go install github.com/uudashr/gopkgs/v2/cmd/gopkgs@latest 2>&1
RUN go install golang.org/x/tools/gopls@latest 2>&1
RUN go install honnef.co/go/tools/cmd/staticcheck@latest 2>&1
RUN go install github.com/cweill/gotests/gotests@latest 2>&1
RUN go install github.com/fatih/gomodifytags@latest 2>&1
RUN go install github.com/josharian/impl@latest 2>&1
RUN go install github.com/haya14busa/goplay/cmd/goplay@latest 2>&1
RUN go install github.com/go-delve/delve/cmd/dlv@latest 2>&1

RUN chmod -R a+rwX /go/pkg && rm -rf /go/src/*

RUN wget -O hivemind.gz https://github.com/DarthSim/hivemind/releases/download/v1.1.0/hivemind-v1.1.0-linux-amd64.gz \
  && gunzip --stdout hivemind.gz > /usr/local/bin/hivemind \
  && chmod +x /usr/local/bin/hivemind \
  && rm hivemind.gz

RUN wget -O overmind.gz https://github.com/DarthSim/overmind/releases/download/v2.4.0/overmind-v2.4.0-linux-amd64.gz \
  && gunzip --stdout overmind.gz > /usr/local/bin/overmind \
  && chmod +x /usr/local/bin/overmind \
  && rm overmind.gz

ADD install-node-red.sh /root/install-node-red.sh

# needed by nod-red install script
ARG TARGETARCH
RUN /root/install-node-red.sh \
  && rm /root/install-node-red.sh

ADD ./src/js/node-red /root/.node-red/nodes/ot-sim
RUN cd /root/.node-red/nodes/ot-sim && npm install

RUN wget -O snap7.7z https://versaweb.dl.sourceforge.net/project/snap7/1.4.2/snap7-full-1.4.2.7z \
  && 7zz -o/tmp x snap7.7z && rm snap7.7z \
  && make -C /tmp/snap7-full-1.4.2/build/unix -f x86_64_linux.mk install LibInstall=/usr/local/lib

RUN python3 -m pip install --break-system-packages opendssdirect.py~=0.8.4
