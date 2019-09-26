/* Copyright 2019 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow_graphics/rendering/opengl/egl_offscreen_context.h"

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "GL/gl/include/GLES3/gl32.h"

namespace {

TEST(EglOffscreenContextTest, TestCreate) {
  std::unique_ptr<EGLOffscreenContext> context;

  TF_CHECK_OK(EGLOffscreenContext::Create(&context, 800, 600));
}

TEST(EglOffscreenContextTest, TestMakeCurrentWorks) {
  std::unique_ptr<EGLOffscreenContext> context1;
  std::unique_ptr<EGLOffscreenContext> context2;

  TF_CHECK_OK(EGLOffscreenContext::Create(&context1, 800, 600));
  TF_CHECK_OK(EGLOffscreenContext::Create(&context2, 400, 100));
  TF_CHECK_OK(context1->MakeCurrent());
  TF_CHECK_OK(context2->MakeCurrent());
}

TEST(EglOffscreenContextTest, TestRelease) {
  std::unique_ptr<EGLOffscreenContext> context;

  TF_CHECK_OK(EGLOffscreenContext::Create(&context, 800, 600));
  TF_CHECK_OK(context->MakeCurrent());
  TF_CHECK_OK(context->Release());
}

TEST(EglOffscreenContextTest, TestRenderClear) {
  std::unique_ptr<EGLOffscreenContext> context;
  const float kRed = 0.1;
  const float kGreen = 0.2;
  const float kBlue = 0.3;
  const float kAlpha = 1.0;
  const int kWidth = 10;
  const int kHeight = 5;
  std::vector<uint8> pixels(kWidth * kHeight * 4);

  TF_CHECK_OK(EGLOffscreenContext::Create(&context, kWidth, kHeight));
  TF_CHECK_OK(context->MakeCurrent());
  glClearColor(kRed, kGreen, kBlue, kAlpha);
  glClear(GL_COLOR_BUFFER_BIT);
  ASSERT_EQ(glGetError(), GL_NO_ERROR);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
  ASSERT_EQ(glGetError(), GL_NO_ERROR);

  for (int index = 0; index < kWidth * kHeight; ++index) {
    ASSERT_EQ(pixels[index * 4], uint8(kRed * 255.0));
    ASSERT_EQ(pixels[index * 4 + 1], uint8(kGreen * 255.0));
    ASSERT_EQ(pixels[index * 4 + 2], uint8(kBlue * 255.0));
    ASSERT_EQ(pixels[index * 4 + 3], uint8(kAlpha * 255.0));
  }
  TF_CHECK_OK(context->Release());
}

}  // namespace
