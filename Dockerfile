FROM golang:1.23-alpine AS build
WORKDIR /app
COPY . .
RUN go build -o vdxcli ./cmd/vdxcli

FROM scratch

COPY --from=build /app/vdxcli /vdxcli
ENTRYPOINT ["/vdxcli"]
