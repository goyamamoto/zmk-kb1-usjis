# Minimal host-toolchain image for the native_posix_64 simulation.
# Everything is pinned: the base image by digest, apt packages by version from
# a dated snapshot.debian.org archive (the live bookworm suite keeps only the
# newest version of each package, so plain version pins break on every point
# release), Python packages by version and sha256. Built by
# scripts/run-simulation.sh (network needed once); the simulation itself then
# runs with --network none.
FROM python@sha256:1d52838af602b4b5a831beb13a0e4d073280665ea7be7f69ce2382f29c5a613f
# python:3.8-slim-bookworm, linux/amd64 (Python 3.8 is EOL; kept because the
# digest pin makes it reproducible and Zephyr 3.2 requires only >= 3.8).

ARG DEBIAN_SNAPSHOT=20260921T000000Z
RUN rm -f /etc/apt/sources.list.d/debian.sources /etc/apt/sources.list \
 && printf 'deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/%s bookworm main\n' "$DEBIAN_SNAPSHOT" > /etc/apt/sources.list \
 && printf 'deb [check-valid-until=no] https://snapshot.debian.org/archive/debian-security/%s bookworm-security main\n' "$DEBIAN_SNAPSHOT" >> /etc/apt/sources.list \
 && printf 'Acquire::Retries "5";\nAcquire::http::Dl-Limit "0";\n' > /etc/apt/apt.conf.d/80-snapshot \
 && apt-get -o Acquire::Check-Valid-Until=false update \
 && apt-get install -y --no-install-recommends \
      gcc-12=12.2.0-14+deb12u1 \
      cpp-12=12.2.0-14+deb12u1 \
      libgcc-12-dev=12.2.0-14+deb12u1 \
      binutils=2.40-2 \
      libc6-dev=2.36-9+deb12u14 \
      libc6=2.36-9+deb12u14 \
      libc-dev-bin=2.36-9+deb12u14 \
      linux-libc-dev=6.1.187-1 \
      git=1:2.39.5-0+deb12u3 \
      device-tree-compiler=1.6.1-4+b1 \
 && ln -s gcc-12 /usr/bin/gcc && ln -s cpp-12 /usr/bin/cpp \
 && rm -rf /var/lib/apt/lists/*

# Same versions as the zmkfirmware/zmk-dev-arm image previously used. west is
# installed because Zephyr's CMake looks for it; the harness itself calls cmake.
COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir --require-hashes -r /tmp/requirements.txt \
 && rm /tmp/requirements.txt

# Any uid can run (see --user in run-simulation.sh); HOME is set at run time.
ENV PYTHONDONTWRITEBYTECODE=1
