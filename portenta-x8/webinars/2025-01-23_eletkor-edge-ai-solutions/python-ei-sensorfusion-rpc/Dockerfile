# FROM debian:12.8
FROM debian:12.8-slim

LABEL maintainer="Giampaolo Mancini <g.mancini@arduino.cc>"

RUN set -ex \
    && apt update \
    && apt install -y --no-install-recommends \
        python3-pip \
        python3-requests \
        python3-opencv

WORKDIR /app/python

COPY ./python/requirements.txt .
RUN set -ex \
    && pip install --break-system-packages -r requirements.txt \
    && rm requirements.txt

COPY ./python/main.py .

ENTRYPOINT ["python3", "-u", "main.py"]
