ARG TAG=17.5
FROM postgres:${TAG}

LABEL maintainer="ivan.stoev@gmail.com" \
      description="PostgreSQL with pldebugger, pgvector and itree" \
      version="1.0"
ARG MAJOR_TAG=17

# Use the major version for package names
ARG PG_LIB=postgresql-server-dev-${MAJOR_TAG}
ARG PG_BRANCH=REL_${MAJOR_TAG}_STABLE
ARG ITREE_REPO=https://github.com/chernia/itree.git
# or replace with a fork like https://github.com/ng-galien/pldebugger/tree/print-vars
ARG PLDEBUGGER_REPO=https://github.com/EnterpriseDB/pldebugger.git

# Install build dependencies
RUN echo "Installing packages: $PG_LIB postgresql-${MAJOR_TAG}-unit postgresql-${MAJOR_TAG}-cron" \
    && apt --yes update \
    && apt --yes upgrade \
    && apt --yes install locales \
    && echo "en_US.UTF-8 UTF-8" > /etc/locale.gen \
    && locale-gen en_US.utf8 \
    && apt --yes install pkg-config git build-essential libreadline-dev zlib1g-dev bison libkrb5-dev flex libicu-dev $PG_LIB postgresql-${MAJOR_TAG}-unit postgresql-${MAJOR_TAG}-cron

# Clone postgres and configure
RUN cd /usr/src/ \
    && git clone -b $PG_BRANCH --single-branch https://github.com/postgres/postgres.git \
    && cd postgres \
    && ./configure --with-icu 

# Build pldebugger
RUN cd /usr/src/postgres/contrib \
    && git clone -b master --single-branch $PLDEBUGGER_REPO \
    && cd pldebugger \
    && make clean && make USE_PGXS=1 && make USE_PGXS=1 install

# Build pgvector
RUN cd /tmp \
    && git clone --branch v0.8.0 --depth 1 https://github.com/pgvector/pgvector.git \
    && cd pgvector \
    && make \
    && make install \
    && rm -rf /tmp/pgvector

# Build itree
RUN cd /usr/src/postgres/contrib \
    && git clone $ITREE_REPO \
    && cd itree \
    && make clean && make USE_PGXS=1 && make USE_PGXS=1 install

RUN rm -r /usr/src/postgres \
    && apt --yes remove --purge git build-essential libreadline-dev zlib1g-dev bison libkrb5-dev flex $PG_LIB \
    && apt --yes autoremove \
    && apt --yes clean

RUN echo "shared_preload_libraries='plugin_debugger,pg_stat_statements'" >> /usr/share/postgresql/postgresql.conf.sample