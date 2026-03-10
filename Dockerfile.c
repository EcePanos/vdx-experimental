FROM alpine:3.20 AS build
RUN apk add --no-cache build-base cmake
WORKDIR /app
COPY . .
RUN rm -rf cver/build \
    && cmake -S cver -B /tmp/cver-build \
       -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_EXE_LINKER_FLAGS="-static" \
    && cmake --build /tmp/cver-build \
    && strip /tmp/cver-build/vdx_c

FROM scratch
COPY --from=build /tmp/cver-build/vdx_c /vdx_c
ENTRYPOINT ["/vdx_c"]
