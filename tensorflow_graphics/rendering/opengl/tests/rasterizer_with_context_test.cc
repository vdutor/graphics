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
#include "tensorflow_graphics/rendering/opengl/rasterizer_with_context.h"

#include <memory>

#include "gtest/gtest.h"
#include "tensorflow_graphics/rendering/opengl/thread_safe_resource_pool.h"
#include "tensorflow/core/lib/core/status.h"

namespace {

const std::string kEmptyShaderCode =
    "#version 430\n"
    "void main() { }\n";

const std::string kFragmentShaderCode =
    "#version 420\n"
    "\n"
    "in layout(location = 0) vec3 position;\n"
    "in layout(location = 1) vec3 normal;\n"
    "in layout(location = 2) vec2 bar_coord;\n"
    "in layout(location = 3) float tri_id;\n"
    "\n"
    "out vec4 output_color;\n"
    "\n"
    "void main() {\n"
    "  output_color = vec4(bar_coord, tri_id, position.z);\n"
    "}\n";

const std::string kGeometryShaderCode =
    "#version 430\n"
    "\n"
    "uniform mat4 view_projection_matrix;\n"
    "\n"
    "layout(points) in;\n"
    "layout(triangle_strip, max_vertices=3) out;\n"
    "\n"
    "out layout(location = 0) vec3 position;\n"
    "out layout(location = 1) vec3 normal;\n"
    "out layout(location = 2) vec2 bar_coord;\n"
    "out layout(location = 3) float tri_id;\n"
    "\n"
    "in int gl_PrimitiveIDIn;\n"
    "layout(binding=0) buffer triangular_mesh { float mesh_buffer[]; };\n"
    "\n"
    "vec3 get_vertex_position(int i) {\n"
    "  int o = gl_PrimitiveIDIn * 9 + i * 3;\n"
    "  return vec3(mesh_buffer[o + 0], mesh_buffer[o + 1], mesh_buffer[o + "
    "2]);\n"
    "}\n"
    "\n"
    "bool is_back_facing(vec3 v0, vec3 v1, vec3 v2) {\n"
    "  vec4 tv0 = view_projection_matrix * vec4(v0, 1.0);\n"
    "  vec4 tv1 = view_projection_matrix * vec4(v1, 1.0);\n"
    "  vec4 tv2 = view_projection_matrix * vec4(v2, 1.0);\n"
    "  tv0 /= tv0.w;\n"
    "  tv1 /= tv1.w;\n"
    "  tv2 /= tv2.w;\n"
    "  vec2 a = (tv1.xy - tv0.xy);\n"
    "  vec2 b = (tv2.xy - tv0.xy);\n"
    "  return (a.x * b.y - b.x * a.y) <= 0;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "  vec3 v0 = get_vertex_position(0);\n"
    "  vec3 v1 = get_vertex_position(1);\n"
    "  vec3 v2 = get_vertex_position(2);\n"
    "\n"
    "  // Cull back-facing triangles.\n"
    "  if (is_back_facing(v0, v1, v2)) {\n"
    "    return;\n"
    "  }\n"
    "\n"
    "  normal = normalize(cross(v1 - v0, v2 - v0));\n"
    "\n"
    "  vec3 positions[3] = {v0, v1, v2};\n"
    "  for (int i = 0; i < 3; ++i) {\n"
    "    // gl_Position is a pre-defined size 4 output variable\n"
    "    gl_Position = view_projection_matrix * vec4(positions[i], 1);\n"
    "    bar_coord = vec2(i==0 ? 1 : 0, i==1 ? 1 : 0);\n"
    "    tri_id = gl_PrimitiveIDIn;\n"
    "\n"
    "    position = positions[i];\n"
    "    EmitVertex();\n"
    "  }\n"
    "  EndPrimitive();\n"
    "}\n";

TEST(RasterizerWithContextTest, TestCreate) {
  constexpr int kWidth = 5;
  constexpr int kHeight = 5;
  std::unique_ptr<RasterizerWithContext<float>> rasterizer_with_context;

  TF_CHECK_OK(RasterizerWithContext<float>::Create(
      kWidth, kHeight, kEmptyShaderCode, kEmptyShaderCode, kEmptyShaderCode,
      &rasterizer_with_context));
}

TEST(RasterizerWithContextTest, TestRenderSingleThread) {
  constexpr int kWidth = 5;
  constexpr int kHeight = 5;
  constexpr float kClearRed = 0.1;
  constexpr float kClearGreen = 0.2;
  constexpr float kClearBlue = 0.3;
  constexpr int kNumRenders = 100;
  constexpr int kNumVertices = 0;
  std::unique_ptr<RasterizerWithContext<float>> rasterizer_with_context;

  TF_CHECK_OK(RasterizerWithContext<float>::Create(
      kWidth, kHeight, kEmptyShaderCode, kGeometryShaderCode,
      kFragmentShaderCode, &rasterizer_with_context, kClearRed, kClearGreen,
      kClearBlue));

  for (int i = 0; i < kNumRenders; ++i) {
    std::vector<float> rendering_result(kWidth * kHeight * 4);

    TF_CHECK_OK(rasterizer_with_context->Render(
        kNumVertices, absl::MakeSpan(rendering_result)));
    for (int p = 0; p < kWidth * kHeight; ++p) {
      EXPECT_EQ(rendering_result[4 * p], kClearRed);
      EXPECT_EQ(rendering_result[4 * p + 1], kClearGreen);
      EXPECT_EQ(rendering_result[4 * p + 2], kClearBlue);
    }
  }
}

constexpr float kIncrementRed = 0.001;
constexpr float kIncrementGreen = 0.002;
constexpr float kIncrementBlue = 0.003;

template <int kWidth, int kHeight>
static tensorflow::Status rasterizer_with_context_creator(
    std::unique_ptr<RasterizerWithContext<float>> *resource) {
  static float red_clear = 0.0;
  static float green_clear = 0.0;
  static float blue_clear = 0.0;

  red_clear += kIncrementRed;
  green_clear += kIncrementGreen;
  blue_clear += kIncrementBlue;
  return RasterizerWithContext<float>::Create(
      kWidth, kHeight, kEmptyShaderCode, kGeometryShaderCode,
      kFragmentShaderCode, resource, red_clear, green_clear, blue_clear);
}

class MemberRoutineWrapper {
 public:
  MemberRoutineWrapper(std::unique_ptr<RasterizerWithContext<float>> rasterizer,
                       int &num_successes, int num_vertices,
                       std::vector<float> &result)
      : num_successes_(num_successes),
        num_vertices_(num_vertices),
        rasterizer_(std::move(rasterizer)),
        result_(result) {}

  ~MemberRoutineWrapper() { rasterizer_.reset(); }

  void *wrapped_routine() {
    if (rasterizer_->Render(num_vertices_, absl::MakeSpan(result_)) ==
        tensorflow::Status::OK())
      ++num_successes_;
    return nullptr;
  }

  int &num_successes_;
  int num_vertices_;
  std::unique_ptr<RasterizerWithContext<float>> rasterizer_;
  std::vector<float> &result_;
};

template <typename T>
void *StartMemberRoutineWrapper(void *instance) {
  T *routine_wrapper = reinterpret_cast<T *>(instance);
  return routine_wrapper->wrapped_routine();
}

TEST(RasterizerWithContextTest, TestRenderGeometry) {
  const std::vector<float> kViewProjectionMatrix = {
      -1.73205, 0.0, 0.0,     0.0, 0.0, 1.73205, 0.0,      0.0,
      0.0,      0.0, 1.22222, 1.0, 0.0, 0.0,     -2.22222, 0.0};
  std::unique_ptr<RasterizerWithContext<float>> rasterizer;
  const int kWidth = 3;
  const int kHeight = 3;

  TF_CHECK_OK(RasterizerWithContext<float>::Create(
      kWidth, kHeight, kEmptyShaderCode, kGeometryShaderCode,
      kFragmentShaderCode, &rasterizer));
  TF_CHECK_OK(
      rasterizer->SetUniformMatrix("view_projection_matrix", 4, 4, false,
                                   absl::MakeConstSpan(kViewProjectionMatrix)));

  std::vector<float> rendering_result(kWidth * kHeight * 4);
  for (float depth = 2; depth < 5; ++depth) {
    std::vector<const float> geometry = {-10.0, 10.0, depth, 10.0, 10.0,
                                         depth, 0.0,  -10.0, depth};
    TF_CHECK_OK(rasterizer->SetShaderStorageBuffer(
        "triangular_mesh", absl::MakeConstSpan(geometry)));
    const int kNumVertices = geometry.size() / 3;
    TF_CHECK_OK(
        rasterizer->Render(kNumVertices, absl::MakeSpan(rendering_result)));

    for (int i = 0; i < kWidth * kHeight; ++i) {
      EXPECT_EQ(rendering_result[4 * i + 2], 0.0);
      EXPECT_EQ(rendering_result[4 * i + 3], depth);
    }
  }
}

TEST(RasterizerWithContextTest, TestRenderMultiThread) {
  constexpr int kNumThreads = 50;
  constexpr int kWidth = 10;
  constexpr int kHeight = 10;
  constexpr int pixel_query = kWidth * kHeight / 2;
  std::array<pthread_t, kNumThreads> threads;
  std::array<int, kNumThreads> num_successes;
  std::array<std::vector<float>, kNumThreads> buffers;
  std::array<MemberRoutineWrapper *, kNumThreads> wrappers;

  for (int i = 0; i < kNumThreads; ++i) {
    buffers[i].resize(kWidth * kHeight * 4);
    num_successes[i] = 0;
    std::unique_ptr<RasterizerWithContext<float>> rasterizer;
    TF_CHECK_OK(
        (rasterizer_with_context_creator<kWidth, kHeight>(&rasterizer)));
    wrappers[i] = new MemberRoutineWrapper(std::move(rasterizer),
                                           num_successes[i], 0, buffers[i]);
    EXPECT_EQ(0, pthread_create(&threads[i], NULL,
                                StartMemberRoutineWrapper<MemberRoutineWrapper>,
                                wrappers[i]));
  }

  // Wait for each thread to be done, accumulate values from the rendered
  // images.
  float sum_r = 0.0f;
  float sum_g = 0.0f;
  float sum_b = 0.0f;
  for (int i = 0; i < kNumThreads; ++i) {
    pthread_join(threads[i], NULL);
    delete wrappers[i];
    ASSERT_EQ(num_successes[i], 1);
    sum_r += buffers[i][4 * pixel_query];
    sum_g += buffers[i][4 * pixel_query + 1];
    sum_b += buffers[i][4 * pixel_query + 2];
  }

  // Check that the accumulated value match the expected value.
  float arithmetic_sum = kNumThreads * (kNumThreads + 1) / 2;
  EXPECT_NEAR(sum_r / kIncrementRed, arithmetic_sum, 0.1);
  EXPECT_NEAR(sum_g / kIncrementGreen, arithmetic_sum, 0.1);
  EXPECT_NEAR(sum_b / kIncrementBlue, arithmetic_sum, 0.1);
}

TEST(RasterizerWithContextTest, TestRenderMultiThreadLoop) {
  constexpr int kNumThreads = 50;
  constexpr int kNumLoops = 10;
  constexpr int kWidth = 10;
  constexpr int kHeight = 10;
  typedef float buffer_type;
  std::array<pthread_t, kNumThreads> threads;
  std::array<int, kNumThreads> num_successes;
  std::array<std::vector<buffer_type>, kNumThreads> buffers;
  std::array<MemberRoutineWrapper *, kNumThreads> wrappers;

  // Launch all the threads.
  for (int i = 0; i < kNumThreads; ++i) {
    buffers[i].resize(kWidth * kHeight * 4);
    num_successes[i] = 0;
    std::unique_ptr<RasterizerWithContext<float>> rasterizer;
    TF_CHECK_OK(
        (rasterizer_with_context_creator<kWidth, kHeight>(&rasterizer)));
    wrappers[i] = new MemberRoutineWrapper(std::move(rasterizer),
                                           num_successes[i], 0, buffers[i]);
    EXPECT_EQ(0, pthread_create(&threads[i], NULL,
                                StartMemberRoutineWrapper<MemberRoutineWrapper>,
                                wrappers[i]));
  }

  for (int l = 0; l < kNumLoops; ++l) {
    // Wait for all threads to terminate and check that all rendering operations
    // succeeded.
    for (int i = 0; i < kNumThreads; ++i) {
      pthread_join(threads[i], NULL);
      ASSERT_EQ(num_successes[i], l + 1);
    }
    // Launch another set of rendering threads.
    for (int i = 0; i < kNumThreads; ++i) {
      ASSERT_EQ(0,
                pthread_create(&threads[i], NULL,
                               StartMemberRoutineWrapper<MemberRoutineWrapper>,
                               wrappers[i]));
    }
  }

  // Wait for all threads to terminate and check that all rendering operations
  // succeeded.
  for (int i = 0; i < kNumThreads; ++i) {
    pthread_join(threads[i], NULL);
    ASSERT_EQ(num_successes[i], kNumLoops + 1);
    delete wrappers[i];
  }
}

}  // namespace
