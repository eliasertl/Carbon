// Carbon Gallery for Android: the shared Gallery (Examples/Gallery) on Carbon's OpenGL ES backend, in a
// GameActivity. GameActivity calls android_main on a thread of its own when the activity is created, and the
// function returns when it is destroyed. Docs/Building.md, "Android", explains how to build and run it.

#include <jni.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "AndroidHost.h"

void android_main(android_app* app)
{
    AndroidGallery::AndroidHost host(app);
    host.Run();
}

// MainActivity.nativeOnBack: Back was pressed while the app handles it.
extern "C" JNIEXPORT void JNICALL Java_io_github_eliasertl_carbon_gallery_MainActivity_nativeOnBack(JNIEnv*, jobject)
{
    AndroidGallery::AndroidHost::RequestBack();
}
