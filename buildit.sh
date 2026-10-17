export SYSROOT=$PREFIX/opt/ndk-multilib/aarch64-linux-android

aarch64-linux-android-clang -shared -fPIC -O2 \
  -I. \
  -isystem $SYSROOT/include \
  -isystem $SYSROOT/sysroot/usr/include \
  main.cpp android_native_app_glue.c \
  -L$SYSROOT/lib -lEGL -lGLESv1_CM -landroid -llog \
  -o libnative-activity.so

rm -rf staging
mkdir -p staging/lib/arm64-v8a
cp libnative-activity.so staging/lib/arm64-v8a/
cd staging && zip -r ../base.apk lib && cd ..

zipalign -f 4 base.apk aligned.apk
apksigner sign --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
  --out ColorFlash.apk aligned.apk

adb install -r ColorFlash.apk
adb shell am start -n com.example.colorflash/android.app.NativeActivity