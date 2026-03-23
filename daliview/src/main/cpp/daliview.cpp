/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>

#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <dali/public-api/events/key-event.h>
#include <dali/devel-api/adaptor-framework/application-devel.h>
#include <dali/devel-api/events/key-event-devel.h>
#include <dali/devel-api/events/touch-point.h>
#include <dali/integration-api/debug.h>
#include <dali/integration-api/adaptor-framework/adaptor.h>
#include <dali/integration-api/adaptor-framework/android/android-framework.h>

void ExtractAsset( AAssetManager* assetManager, std::string assetPath, std::string filePath )
{
  AAsset* asset = AAssetManager_open( assetManager, assetPath.c_str(), AASSET_MODE_BUFFER );
  if( asset )
  {
    size_t length = AAsset_getLength( asset ) + 1;

    char* buffer = new char[ length ];
    length = AAsset_read( asset, buffer, length );

    FILE* file = fopen( filePath.c_str(), "wb" );
    if( file )
    {
      fwrite( buffer, 1, length, file );
      fclose( file );
    }

    delete[] buffer;
    AAsset_close( asset );
  }
}

void ExtractAssetsDir( AAssetManager* assetManager, std::string assetDirPath, std::string filesDirPath )
{
  AAssetDir* assetDir = AAssetManager_openDir( assetManager, assetDirPath.c_str() );
  if( assetDir )
  {
    if( mkdir( filesDirPath.c_str(), S_IRWXU ) != -1 )
    {
      const char *filename = NULL;
      std::string assetPath = assetDirPath + "/";
      while ( ( filename = AAssetDir_getNextFileName( assetDir ) ) != NULL )
      {
        ExtractAsset( assetManager, assetPath + filename, filesDirPath + "/" + filename );
      }
    }

    AAssetDir_close( assetDir );
  }
}

void ExtractFontConfig( AAssetManager* assetManager, std::string assetFontConfig, std::string filesPath, std::string fontsPath )
{
  AAsset* asset = AAssetManager_open( assetManager, assetFontConfig.c_str(), AASSET_MODE_BUFFER );
  if( asset )
  {
    size_t length = AAsset_getLength( asset ) + 1;

    char* buffer = new char[ length ];
    length = AAsset_read( asset, buffer, length );

    std::string fontConfig = std::string( buffer, length );
    int i = fontConfig.find("~");
    if( i != std::string::npos ) {
      fontConfig.replace(i, 1, filesPath);
    }

    std::string fontsFontConfig = fontsPath;
    FILE* file = fopen( fontsFontConfig.c_str(), "wb" );
    if( file )
    {
      fwrite( fontConfig.c_str(), 1, fontConfig.size(), file );
      fclose( file );
    }

    delete[] buffer;
    AAsset_close( asset );
  }
}

extern "C" void FcConfigPathInit(const char* path, const char* file);

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnConfigure(JNIEnv* jenv, jobject obj, jobject assetManager, jstring filesPath )
{
  DALI_LOG_RELEASE_INFO( "nativeOnConfigure" );
  Dali::Integration::AndroidFramework::New();

  JavaVM* jvm = nullptr;
  jenv->GetJavaVM( &jvm );
  Dali::Integration::AndroidFramework::Get().SetJVM( jvm );

  AAssetManager* am = AAssetManager_fromJava( jenv, assetManager );
  Dali::Integration::AndroidFramework::Get().SetApplicationAssets( am );

  AConfiguration* configuration = AConfiguration_new();
  AConfiguration_fromAssetManager( configuration, am );
  Dali::Integration::AndroidFramework::Get().SetApplicationConfiguration( configuration );

  jboolean isCopy = false;
  const char* cstr = jenv->GetStringUTFChars( filesPath, &isCopy );
  std::string filesDir = std::string( cstr );
  jenv->ReleaseStringUTFChars( filesPath, cstr );

  std::string fontconfigPath = filesDir + "/fonts";
  setenv( "FONTCONFIG_PATH", fontconfigPath.c_str(), 1 );
  std::string fontconfigFile = fontconfigPath + "/fonts.conf";
  setenv( "FONTCONFIG_FILE", fontconfigFile.c_str(), 1 );
  FcConfigPathInit( fontconfigPath.c_str(), fontconfigFile.c_str() );

  struct stat st = { 0 };
  if( stat( fontconfigPath.c_str(), &st ) == -1 )
  {
    mkdir( fontconfigPath.c_str(), S_IRWXU );
    ExtractFontConfig( am, "fonts/fonts.conf", filesDir, fontconfigPath + "/fonts.conf");
    ExtractFontConfig( am, "fonts/fonts.dtd", filesDir, fontconfigPath + "/fonts.dtd" );
    ExtractFontConfig( am, "fonts/local.conf", filesDir, fontconfigPath + "/local.conf" );
    ExtractAssetsDir( am, "fonts/dejavu", fontconfigPath + "/dejavu" );
    ExtractAssetsDir( am, "fonts/tizen", fontconfigPath + "/tizen" );
    ExtractAssetsDir( am, "fonts/bitmap", fontconfigPath + "/bitmap" );
  }
}

extern "C" JNIEXPORT jlong JNICALL Java_com_sec_daliview_DaliView_nativeOnCreate(JNIEnv* jenv, jobject obj)
{
  DALI_LOG_RELEASE_INFO( "nativeOnCreate" );

  // Default, creates empty app
  Dali::Application application = Dali::Application::New( 0, NULL );
  application.GetObjectPtr()->Reference();
  return reinterpret_cast<jlong>( application.GetObjectPtr() );
}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnResume(JNIEnv* jenv, jobject obj, jlong handle)
{
  DALI_LOG_RELEASE_INFO( "nativeOnResume handle(%lld)", handle );

  if( handle )
  {
    Dali::Integration::AndroidFramework::Get().OnResume();
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnPause(JNIEnv* jenv, jobject obj, jlong handle)
{
  DALI_LOG_RELEASE_INFO( "nativeOnPause handle(%lld)", handle );

  if( handle )
  {
    Dali::Integration::AndroidFramework::Get().OnPause();
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeSetSurface(JNIEnv* jenv, jobject obj, jlong handle, jobject surface)
{
  DALI_LOG_RELEASE_INFO( "nativeSetSurface handle(%lld)", handle );

  if( handle )
  {
    ANativeWindow* oldWindow = Dali::Integration::AndroidFramework::Get().GetApplicationWindow();
    ANativeWindow* newWindow = nullptr;
    if( surface )
    {
      newWindow = ANativeWindow_fromSurface(jenv, surface);
    }

    DALI_LOG_WARNING( "oldWindow(%p), newWindow(%p)", oldWindow, newWindow );
    if( newWindow != oldWindow )
    {
      Dali::Integration::AndroidFramework::Get().SetApplicationWindow( newWindow );

      if( newWindow )
      {
        Dali::Integration::AndroidFramework::Get().OnWindowCreated( newWindow );
      }
      else
      {
        Dali::Integration::AndroidFramework::Get().OnWindowDestroyed( oldWindow );
      }

      if( oldWindow )
      {
        ANativeWindow_release( oldWindow );
      }
    }
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnTouchEvent(JNIEnv* jenv, jobject obj, jlong handle, jint deviceId, jint action, jfloat x, jfloat y, jlong timestamp)
{
  DALI_LOG_RELEASE_INFO( "nativeOnTouchEvent handle(%lld), deviceId(%d), action(%d), x(%f), y(%f), timestamp(%lld)", handle, deviceId, action, x, y, timestamp );

  Dali::PointState::Type state = Dali::PointState::DOWN;
  switch ( action & AMOTION_EVENT_ACTION_MASK )
  {
    case AMOTION_EVENT_ACTION_DOWN:
      break;
    case AMOTION_EVENT_ACTION_UP:
      state = Dali::PointState::UP;
      break;
    case AMOTION_EVENT_ACTION_MOVE:
      state = Dali::PointState::MOTION;
      break;
    case AMOTION_EVENT_ACTION_CANCEL:
      state = Dali::PointState::INTERRUPTED;
      break;
    case AMOTION_EVENT_ACTION_OUTSIDE:
      state = Dali::PointState::LEAVE;
      break;
  }

  Dali::TouchPoint point( deviceId, state, x, y );
  Dali::Adaptor::Get().FeedTouchPoint( point, timestamp );
}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnKeyEvent(JNIEnv* jenv, jobject obj, jlong handle, jint deviceId, jint action, jint keyCode, jlong timestamp)
{
  DALI_LOG_RELEASE_INFO( "nativeOnKeyEvent handle(%lld), deviceId(%d), action(%d), keyCode(%d), timestamp(%lld)", handle, deviceId, action, keyCode, timestamp );

  Dali::KeyEvent::State state = Dali::KeyEvent::DOWN;
  switch ( action )
  {
    case AKEY_EVENT_ACTION_DOWN:
      break;
    case AKEY_EVENT_ACTION_UP:
      state = Dali::KeyEvent::UP;
      break;
  }

  switch( keyCode )
  {
    case 4:
      Dali::KeyEvent keyEvent = Dali::DevelKeyEvent::New("XF86Back", "", "", keyCode, 0, timestamp, state, "", "", Dali::Device::Class::NONE, Dali::Device::Subclass::NONE);
      Dali::Adaptor::Get().FeedKeyEvent( keyEvent );
      break;
    default:
      Dali::KeyEvent keyEvent = Dali::DevelKeyEvent::New("", "", "", keyCode, 0, timestamp, state, "", "", Dali::Device::Class::NONE, Dali::Device::Subclass::NONE);
      Dali::Adaptor::Get().FeedKeyEvent( keyEvent );
      break;
  }

}

extern "C" JNIEXPORT void JNICALL Java_com_sec_daliview_DaliView_nativeOnFinalize(JNIEnv* jenv, jobject obj, jlong handle)
{
  DALI_LOG_RELEASE_INFO( "nativeOnFinalize handle(%lld)", handle );
  // We don't have an option to cleanup existing DALi instance yet - needs major refactoring in DALi
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_sec_daliview_DaliView_nativeOnCallback(JNIEnv* jenv, jclass clazz, jlong callback, jlong callbackData)
{
  DALI_LOG_RELEASE_INFO( "nativeOnCallback callback(%lld), callbackData(%lld)", callback, callbackData );
  bool result = reinterpret_cast<bool(*)(void*)>( callback )( reinterpret_cast<void*>( callbackData ) );
  return ( result ) ? JNI_TRUE : JNI_FALSE;
}
