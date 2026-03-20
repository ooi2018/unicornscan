# PostgREST with wget for proper HTTP healthchecks
# The official postgrest image is scratch-based with no utilities
FROM alpine:3.20

# Install wget for healthchecks and ca-certificates for HTTPS
RUN apk add --no-cache wget ca-certificates libpq

# Download and install PostgREST binary (auto-detect architecture)
ARG POSTGREST_VERSION=12.2.3
RUN ARCH=$(uname -m) \
    && case "$ARCH" in \
         aarch64|arm64) PGREST_ARCH="linux-static-arm64" ;; \
         x86_64|amd64)  PGREST_ARCH="linux-static-x64" ;; \
         *) echo "Unsupported architecture: $ARCH" && exit 1 ;; \
       esac \
    && wget -q https://github.com/PostgREST/postgrest/releases/download/v${POSTGREST_VERSION}/postgrest-v${POSTGREST_VERSION}-${PGREST_ARCH}.tar.xz \
    && tar xJf postgrest-v${POSTGREST_VERSION}-${PGREST_ARCH}.tar.xz \
    && mv postgrest /usr/local/bin/ \
    && rm postgrest-v${POSTGREST_VERSION}-${PGREST_ARCH}.tar.xz \
    && chmod +x /usr/local/bin/postgrest

# Verify installation
RUN postgrest --version && wget --version | head -1

EXPOSE 3000

CMD ["postgrest"]
