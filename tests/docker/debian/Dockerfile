FROM debian:stable-slim as builder

RUN apt update && \
    apt install -y gcc make

RUN mkdir -p /opt/plimit

COPY . /opt/plimit

WORKDIR /opt/plimit

RUN make && \
    make install

FROM debian:stable-slim

COPY --from=builder /usr/local/bin/plimit /usr/local/bin/plimit
COPY tests/docker/scripts/run_stress.sh /usr/local/bin/run_stress.sh

RUN apt update && \
    apt install -y stress procps

RUN chmod u+x /usr/local/bin/plimit && \
    chmod u+x /usr/local/bin/run_stress.sh

CMD ["/usr/local/bin/run_stress.sh"]