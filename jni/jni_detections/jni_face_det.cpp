/*
 * jni_pedestrian_det.cpp using google-style
 *
 *  Created on: Oct 20, 2015
 *      Author: Nanyun
 *
 *  Copyright (c) 2015 Nanyun. All rights reserved.
 */
#include <android/bitmap.h>
#include <jni_common/jni_bitmap2mat.h>
#include <jni_common/jni_primitives.h>
#include <jni_common/jni_fileutils.h>
#include <jni_common/jni_utils.h>
#include <detector.h>
#include <jni.h>

using namespace cv;

extern JNI_VisionDetRet *g_pJNI_VisionDetRet;

namespace
{

#define JAVA_NULL 0
using DetectorPtr = DLibHOGFaceDetector *;

class JNI_FaceDet
{
      public:
        JNI_FaceDet(JNIEnv *env)
        {
                jclass clazz = env->FindClass(CLASSNAME_FACE_DET);
                mNativeContext = env->GetFieldID(clazz, "mNativeFaceDetContext", "J");
                env->DeleteLocalRef(clazz);
        }

        DetectorPtr getDetectorPtrFromJava(JNIEnv *env, jobject thiz)
        {
                DetectorPtr const p = (DetectorPtr)env->GetLongField(thiz, mNativeContext);
                return p;
        }

        void setDetectorPtrToJava(JNIEnv *env, jobject thiz, jlong ptr)
        {
                env->SetLongField(thiz, mNativeContext, ptr);
        }

        jfieldID mNativeContext;
};

// Protect getting/setting and creating/deleting pointer between java/native
std::mutex gLock;

std::shared_ptr<JNI_FaceDet> getJNI_FaceDet(JNIEnv *env)
{
        static std::once_flag sOnceInitflag;
        static std::shared_ptr<JNI_FaceDet> sJNI_FaceDet;
        std::call_once(sOnceInitflag, [env]() {
                sJNI_FaceDet = std::make_shared<JNI_FaceDet>(env);
        });
        return sJNI_FaceDet;
}

DetectorPtr const getDetectorPtr(JNIEnv *env, jobject thiz)
{
        std::lock_guard<std::mutex> lock(gLock);
        return getJNI_FaceDet(env)->getDetectorPtrFromJava(env, thiz);
}

// The function to set a pointer to java and delete it if newPtr is empty
void setDetectorPtr(JNIEnv *env, jobject thiz, DetectorPtr newPtr)
{
        std::lock_guard<std::mutex> lock(gLock);
        DetectorPtr oldPtr = getJNI_FaceDet(env)->getDetectorPtrFromJava(env, thiz);
        if (oldPtr != JAVA_NULL)
        {
                DLOG(INFO) << "setMapManager delete old ptr : " << oldPtr;
                delete oldPtr;
        }

        if (newPtr != JAVA_NULL)
        {
                DLOG(INFO) << "setMapManager set new ptr : " << newPtr;
        }

        getJNI_FaceDet(env)->setDetectorPtrToJava(env, thiz, (jlong)newPtr);
}

} // end unnamespace

#ifdef __cplusplus
extern "C" {
#endif

#define DLIB_FACE_JNI_METHOD(METHOD_NAME) \
        Java_com_nanyun_dlib_FaceDet_##METHOD_NAME

void JNIEXPORT
    DLIB_FACE_JNI_METHOD(jniNativeClassInit)(JNIEnv *env, jclass _this)
{
}

jobjectArray getDetectResult(JNIEnv *env, DetectorPtr faceDetector,
                             const int &size)
{
        LOG(INFO) << "getFaceRet";
        jobjectArray jDetRetArray = JNI_VisionDetRet::createJObjectArray(env, size);
        for (int i = 0; i < size; i++)
        {
                jobject jDetRet = JNI_VisionDetRet::createJObject(env);
                env->SetObjectArrayElement(jDetRetArray, i, jDetRet);
                dlib::rectangle rect = faceDetector->getResult()[i];
                g_pJNI_VisionDetRet->setRect(env, jDetRet, rect.left(), rect.top(),
                                             rect.right(), rect.bottom());
                g_pJNI_VisionDetRet->setLabel(env, jDetRet, "face");
                std::unordered_map<int, dlib::full_object_detection> &faceShapeMap =
                    faceDetector->getFaceShapeMap();
                if (faceShapeMap.find(i) != faceShapeMap.end())
                {
                        dlib::full_object_detection shape = faceShapeMap[i];
                        for (unsigned long j = 0; j < shape.num_parts(); j++)
                        {
                                int x = shape.part(j).x();
                                int y = shape.part(j).y();
                                // Call addLandmark
                                g_pJNI_VisionDetRet->addLandmark(env, jDetRet, x, y);
                        }
                }
        }
        return jDetRetArray;
}

bool rightRotate(std::vector<std::vector<InnerPoint>> &points)
{
        int size = points.size();
        int frame = 0;
        int mStartNumRight = 0;
        float resizeRatio = 1;
        int mStartNumRightTemp = 0;
        int mStartNumLeftTemp = 0;
        int temp_x;
        int temp_y;
        double RDetect;
        float gap_threshold = 2;

        InnerPoint right_pupil;
        InnerPoint left_pupil;

        for (int i = 0; i < size; i++)
        {
                std::vector<InnerPoint> &face_points = (points[i]);

                // Face landmanr result
                temp_x = 0;
                temp_y = 0;
                for (int j = 36; j < 42; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                left_pupil.x = temp_x / 6;
                left_pupil.y = temp_y / 6;

                temp_x = 0;
                temp_y = 0;

                for (int j = 42; j < 48; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                right_pupil.x = temp_x / 6;
                right_pupil.y = temp_y / 6;

                mStartNumRightTemp = abs(right_pupil.x - face_points[30].x);
                mStartNumLeftTemp = abs(face_points[30].x - left_pupil.x);

                if (fabs(mStartNumRightTemp / (float)mStartNumLeftTemp - 1) < fabs(gap_threshold - 1.0))
                {
                        gap_threshold = mStartNumRightTemp / (float)mStartNumLeftTemp;
                        mStartNumRight = mStartNumRightTemp;
                }
        }
        if (gap_threshold > 1.2 || gap_threshold < 0.8)
        {
                return false;
        }

        for (int i = 0; i < size; i++)
        {
                std::vector<InnerPoint> &face_points = (points[i]);

                // Face landmanr result
                temp_x = 0;
                temp_y = 0;
                for (int j = 36; j < 42; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                left_pupil.x = temp_x / 6;
                left_pupil.y = temp_y / 6;

                temp_x = 0;
                temp_y = 0;

                for (int j = 42; j < 48; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                right_pupil.x = temp_x / 6;
                right_pupil.y = temp_y / 6;

                face_points[30].x = (int)(face_points[30].x * resizeRatio);

                RDetect = abs(right_pupil.x - face_points[30].x) / (float)(mStartNumRight);
                if (RDetect < 0.5)
                {
                        frame += 1;
                }
        }
        int threshold = 2;
        if (threshold < 1)
                threshold = 1;
        if (frame >= threshold)
        {
                return true;
        }
        return false;
}

bool leftRotate(std::vector<std::vector<InnerPoint>> &points)
{
        int size = points.size();
        int frame = 0;
        int mStartNumLeft = 0;
        float resizeRatio = 1;
        int mStartNumRightTemp = 0;
        int mStartNumLeftTemp = 0;
        int temp_x;
        int temp_y;
        double LDetect;
        float gap_threshold = 2;

        InnerPoint right_pupil;
        InnerPoint left_pupil;

        for (int i = 0; i < size; i++)
        {
                std::vector<InnerPoint> &face_points = (points[i]);

                // Face landmanr result
                temp_x = 0;
                temp_y = 0;
                for (int j = 36; j < 42; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                left_pupil.x = temp_x / 6;
                left_pupil.y = temp_y / 6;

                temp_x = 0;
                temp_y = 0;

                for (int j = 42; j < 48; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                right_pupil.x = temp_x / 6;
                right_pupil.y = temp_y / 6;

                mStartNumRightTemp = abs(right_pupil.x - face_points[30].x);
                mStartNumLeftTemp = abs(face_points[30].x - left_pupil.x);

                if (fabs(mStartNumRightTemp / (float)mStartNumLeftTemp - 1) < fabs(gap_threshold - 1.0))
                {
                        gap_threshold = mStartNumRightTemp / (float)mStartNumLeftTemp;
                        mStartNumLeft = mStartNumLeftTemp;
                }
        }

        if (gap_threshold > 1.2 || gap_threshold < 0.8)
        {

                return false;
        }

        for (int i = 0; i < size; i++)
        {
                std::vector<InnerPoint> &face_points = (points[i]);

                // Face landmanr result
                temp_x = 0;
                temp_y = 0;
                for (int j = 36; j < 42; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                left_pupil.x = temp_x / 6;
                left_pupil.y = temp_y / 6;

                temp_x = 0;
                temp_y = 0;

                for (int j = 42; j < 48; j++)
                {
                        temp_x = temp_x + (int)(face_points[j].x * resizeRatio);
                        temp_y = temp_y + (int)(face_points[j].y * resizeRatio);
                }
                right_pupil.x = temp_x / 6;
                right_pupil.y = temp_y / 6;

                face_points[30].x = (int)(face_points[30].x * resizeRatio);

                LDetect = abs(face_points[30].x - left_pupil.x) / (float)(mStartNumLeft);
                if (LDetect < 0.5)
                {
                        frame += 1;
                }
        }
        int threshold = 2;
        if (threshold < 1)
                threshold = 1;
        if (frame >= threshold)
        {
                return true;
        }
        return false;
}

double getDistance(InnerPoint p1, InnerPoint p2)
{
        return sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
}

double ear(InnerPoint p1, InnerPoint p2, InnerPoint p3, InnerPoint p4, InnerPoint p5, InnerPoint p6)
{
        return (getDistance(p2, p6) + getDistance(p3, p5)) / (2 * getDistance(p1, p4));
}

bool eyeClosed(std::vector<std::vector<InnerPoint>> &points)
{
        int size = points.size();
        int frame = 0;
        int mStartNumLeft = 0;
        int mStartNumRight = 0;
        float resizeRatio = 1;

        for (int i = 0; i < size; i++)
        {
                std::vector<InnerPoint> &face_points = points[i];

                face_points[30].x = (int)(face_points[30].x * resizeRatio);
                face_points[48].x = (int)(face_points[48].x * resizeRatio);
                face_points[54].x = (int)(face_points[54].x * resizeRatio);

                face_points[30].y = (int)(face_points[30].y * resizeRatio);
                face_points[48].y = (int)(face_points[48].y * resizeRatio);
                face_points[54].y = (int)(face_points[54].y * resizeRatio);

                // Eye detection
                double rightEAR = ear(face_points[36],
                                      face_points[37], face_points[38],
                                      face_points[39], face_points[40],
                                      face_points[41]) *
                                  resizeRatio;
                double leftEAR = ear(face_points[42],
                                     face_points[43], face_points[44],
                                     face_points[45], face_points[46],
                                     face_points[47]) *
                                 resizeRatio;
                double earAve = (rightEAR + leftEAR) / 2;
                if (earAve < 0.2)
                        frame += 1;
        }
        int threshold = 1;
        if (threshold < 1)
                threshold = 1;
        if (frame >= threshold)
        {
                return true;
        }
        return false;
}

JNIEXPORT jboolean JNICALL
    DLIB_FACE_JNI_METHOD(rightRotateEx)(JNIEnv *env, jobject thiz,
                                        jobjectArray arrayList)
{
        LOG(INFO) << "rightRotateEx";
        jclass java_util_ArrayList = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
        CHECK_NOTNULL(java_util_ArrayList);
        auto java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
        auto java_util_ArrayList_get = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");
        jint size = env->CallIntMethod(arrayList, java_util_ArrayList_size);
        std::vector<std::vector<InnerPoint>> Rlt(size);
        for (jint i = 0; i < size; i++)
        {
                jobject visonRet = env->CallObjectMethod(arrayList, java_util_ArrayList_get, i);
                g_pJNI_VisionDetRet->getLandmark(env, visonRet, Rlt[i]);
                env->DeleteLocalRef(visonRet);
        }

        auto is_right = rightRotate(Rlt);
        LOG(INFO) << "rightRotateEx result: " << is_right;
        return is_right;
}

JNIEXPORT jboolean JNICALL
    DLIB_FACE_JNI_METHOD(leftRotateEx)(JNIEnv *env, jobject thiz,
                                       jobjectArray arrayList)
{
        LOG(INFO) << "leftRotate";
        jclass java_util_ArrayList = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
        CHECK_NOTNULL(java_util_ArrayList);
        auto java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
        auto java_util_ArrayList_get = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");
        jint size = env->CallIntMethod(arrayList, java_util_ArrayList_size);
        std::vector<std::vector<InnerPoint>> Rlt(size);
        for (jint i = 0; i < size; i++)
        {
                jobject visonRet = env->CallObjectMethod(arrayList, java_util_ArrayList_get, i);
                g_pJNI_VisionDetRet->getLandmark(env, visonRet, Rlt[i]);
                env->DeleteLocalRef(visonRet);
        }

        auto is_right = leftRotate(Rlt);
        LOG(INFO) << "rightRotateEx result: " << is_right;
        return is_right;
}

JNIEXPORT jboolean JNICALL
    DLIB_FACE_JNI_METHOD(eyeClosedEx)(JNIEnv *env, jobject thiz,
                                      jobjectArray arrayList)
{
        LOG(INFO) << "leftRotate";
        jclass java_util_ArrayList = static_cast<jclass>(env->NewGlobalRef(env->FindClass("java/util/ArrayList")));
        CHECK_NOTNULL(java_util_ArrayList);
        auto java_util_ArrayList_size = env->GetMethodID(java_util_ArrayList, "size", "()I");
        auto java_util_ArrayList_get = env->GetMethodID(java_util_ArrayList, "get", "(I)Ljava/lang/Object;");
        jint size = env->CallIntMethod(arrayList, java_util_ArrayList_size);
        std::vector<std::vector<InnerPoint>> Rlt(size);
        for (jint i = 0; i < size; i++)
        {
                jobject visonRet = env->CallObjectMethod(arrayList, java_util_ArrayList_get, i);
                g_pJNI_VisionDetRet->getLandmark(env, visonRet, Rlt[i]);
                env->DeleteLocalRef(visonRet);
        }

        auto is_right = eyeClosed(Rlt);
        LOG(INFO) << "rightRotateEx result: " << is_right;
        return is_right;
}

JNIEXPORT jobjectArray JNICALL
    DLIB_FACE_JNI_METHOD(jniDetect)(JNIEnv *env, jobject thiz,
                                    jstring imgPath)
{
        LOG(INFO) << "jniFaceDet";
        const char *img_path = env->GetStringUTFChars(imgPath, 0);
        DetectorPtr detPtr = getDetectorPtr(env, thiz);
        int size = detPtr->det(std::string(img_path));
        env->ReleaseStringUTFChars(imgPath, img_path);
        LOG(INFO) << "det face size: " << size;
        return getDetectResult(env, detPtr, size);
}

JNIEXPORT jobjectArray JNICALL
    DLIB_FACE_JNI_METHOD(jniBitmapDetect)(JNIEnv *env, jobject thiz,
                                          jobject bitmap)
{
        LOG(INFO) << "jniBitmapFaceDet";
        cv::Mat rgbaMat;
        cv::Mat bgrMat;
        jniutils::ConvertBitmapToRGBAMat(env, bitmap, rgbaMat, true);
        cv::cvtColor(rgbaMat, bgrMat, cv::COLOR_RGBA2BGR);
        DetectorPtr detPtr = getDetectorPtr(env, thiz);
        jint size = detPtr->det(bgrMat);
#if 0
  cv::Mat rgbMat;
  cv::cvtColor(bgrMat, rgbMat, cv::COLOR_BGR2RGB);
  cv::imwrite("/sdcard/ret.jpg", rgbaMat);
#endif
        LOG(INFO) << "det face size: " << size;
        return getDetectResult(env, detPtr, size);
}

jint JNIEXPORT JNICALL DLIB_FACE_JNI_METHOD(jniInit)(JNIEnv *env, jobject thiz,
                                                     jstring jLandmarkPath)
{
        LOG(INFO) << "jniInit";
        std::string landmarkPath = jniutils::convertJStrToString(env, jLandmarkPath);
        DetectorPtr detPtr = new DLibHOGFaceDetector(landmarkPath);
        setDetectorPtr(env, thiz, detPtr);
        ;
        return JNI_OK;
}

jint JNIEXPORT JNICALL
    DLIB_FACE_JNI_METHOD(jniDeInit)(JNIEnv *env, jobject thiz)
{
        LOG(INFO) << "jniDeInit";
        setDetectorPtr(env, thiz, JAVA_NULL);
        return JNI_OK;
}

#ifdef __cplusplus
}
#endif
