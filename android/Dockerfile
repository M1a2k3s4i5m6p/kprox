FROM thyrlian/android-sdk:latest

WORKDIR /app
COPY android_app/ .

RUN yes | sdkmanager --licenses

RUN gradle assembleDebug

RUN mkdir -p /output && \
    cp app/build/outputs/apk/debug/app-debug.apk /output/kprox-discovery.apk

CMD ["cp", "/output/kprox-discovery.apk", "/host/"]
