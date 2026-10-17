一个安卓纯native的软件
16种颜色循环闪烁，延迟200毫秒
编译需要:aapt,aapt2,apksigner(源码已含签名)
需要的文件:
android_native_app_glue.h(内置)
android_native_app_glue.c(内置)
android.jar
android sdk里面的EGL,GLES,KHR(自行提取)
编译:bash buildit.sh
源码中内置了android_native_app_glue.h和.c,这两个文件归谷歌所有，使用Apache License 2.0
