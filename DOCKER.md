Docker Configurations
=====================

This is used to describe our docker configurations.

Used to generate the gitlab-ci image:

```
#base immage we are using is debian stretch
	FROM debian:stretch
#set enviorment to noninteractive
	ENV DEBIAN_FRONTEND=noninteractive
#Install needed packages without recommends	
	RUN apt-get update && apt-get install -y \
	build-essential \
	qtbase5-dev \
	qtbase5-dev-tools \
	qt5-qmake \
	qt5-default \
	ca-certificates \
	libtcmalloc-minimal4 \
	libexosip2-11 \
	curl \
	bash \
	libosip2-dev \
	libssl-dev \
	libexosip2-dev \
	libsystemd-dev \
	libqt5sql5-sqlite \
	make \
	cmake \
	ninja-build && \
	apt-get clean
#install the repository and the gitlab-ci   
	RUN curl -L https://packages.gitlab.com/install/repositories/runner/gitlab-ci-multi-runner/script.deb.sh | bash && \
	echo -e "Explanation: Prefer GitLab provided packages over the Debian native ones\nPackage: gitlab-ci-multi-runner\nPin: origin packages.gitlab.com\nPin-Priority: 1001\n" > /etc/apt/preferences.d/pin-gitlab-runner.pref && \
	apt-get install -y --allow-unauthenticated gitlab-ci-multi-runner && \
	rm -rf /var/lib/apt/lists/* 
#link problematic shared object on debian	
	RUN ln -s /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so
#register gitlab runner for this project	
	RUN gitlab-runner register \
	  --url "https://gitlab.com/" \
 	 --registration-token "S6yboJN2yQSexj_vNpp5" \
 	 --description "sipwitch build image" \
 	 --executor "docker" \
 	 --docker-image in1t3r/gitlabqt:build 
```

Used to generate our documetation with a gitlab-ci image:

```
#base immage we are using is debian stretch
	FROM debian:stretch
#set enviorment to noninteractive
	ENV DEBIAN_FRONTEND=noninteractive
#Install needed packages without recommends	
	RUN apt-get update && apt-get install -y \
	build-essential \
	qtbase5-dev \
	qtbase5-dev-tools \
	qt5-qmake \
	qt5-default \
	ca-certificates \
	libtcmalloc-minimal4 \
	libexosip2-11 \
	curl \
	bash \
	libosip2-dev \
	libssl-dev \
	libexosip2-dev \
	libsystemd-dev \
	libqt5sql5-sqlite \
	make 
	RUN apt-get install -y --no-install-recommends texlive-base \
	graphviz \
	texlive-fonts-recommended \
	texlive-latex-recommended \
	texlive-latex-extra \
	doxygen && \
	apt-get clean
#install the repository and the gitlab-ci   
	RUN curl -L https://packages.gitlab.com/install/repositories/runner/gitlab-ci-multi-runner/script.deb.sh | bash && \
	echo -e "Explanation: Prefer GitLab provided packages over the Debian native ones\nPackage: gitlab-ci-multi-runner\nPin: origin packages.gitlab.com\nPin-Priority: 1001\n" > /etc/apt/preferences.d/pin-gitlab-runner.pref && \
	apt-get install -y --allow-unauthenticated gitlab-ci-multi-runner && \
	rm -rf /var/lib/apt/lists/* 
#link problematic shared object on debian	
	RUN ln -s /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so
#register gitlab runner for this project	
	RUN gitlab-runner register \
	  --url "https://gitlab.com/" \
 	 --registration-token "S6yboJN2yQSexj_vNpp5" \
 	 --description "sipwitch documentation image" \
 	 --executor "docker" \
 	 --docker-image in1t3r/gitlabqt:documentation 

```
