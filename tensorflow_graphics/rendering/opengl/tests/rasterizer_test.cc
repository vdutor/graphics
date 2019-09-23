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
#include "tensorflow_graphics/rendering/opengl/rasterizer.h"

#include <vector>

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "absl/types/span.h"
#include "tensorflow_graphics/rendering/opengl/egl_offscreen_context.h"

namespace {

const std::string empty_shader_code =
    "#version 430\n"
    "void main() { }\n";

const std::string fragment_shader_code =
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

const std::string geometry_shader_code =
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

TEST(RasterizerTest, TestCreate) {
  std::unique_ptr<EGLOffscreenContext> context;
  std::unique_ptr<Rasterizer<float>> rasterizer;

  EXPECT_TRUE(EGLOffscreenContext::Create(&context));
  EXPECT_TRUE(context->MakeCurrent());
  EXPECT_TRUE(Rasterizer<float>::Create(3, 2, empty_shader_code,
                                        empty_shader_code, empty_shader_code,
                                        &rasterizer));
}

TEST(RasterizerTest, TestSetShaderStorageBuffer) {
  std::unique_ptr<EGLOffscreenContext> context;
  std::unique_ptr<Rasterizer<float>> rasterizer;

  EXPECT_TRUE(EGLOffscreenContext::Create(&context));
  EXPECT_TRUE(context->MakeCurrent());
  EXPECT_TRUE(Rasterizer<float>::Create(3, 2, empty_shader_code,
                                        empty_shader_code, empty_shader_code,
                                        &rasterizer));

  // Fronto-parallel triangle at depth 1.
  std::vector<const float> geometry = {-1.0, 1.0, 1.0,  1.0, 1.0,
                                       1.0,  0.0, -1.0, 1.0};
  std::string buffer_name = "geometry";
  EXPECT_TRUE(rasterizer->SetShaderStorageBuffer(buffer_name,
                                                 absl::MakeSpan(geometry)));
}

TEST(RasterizerTest, TestSetUniformMatrix) {
  std::unique_ptr<EGLOffscreenContext> context;
  std::unique_ptr<Rasterizer<float>> rasterizer;

  EXPECT_TRUE(EGLOffscreenContext::Create(&context));
  EXPECT_TRUE(context->MakeCurrent());
  EXPECT_TRUE(Rasterizer<float>::Create(3, 2, empty_shader_code,
                                        geometry_shader_code,
                                        fragment_shader_code, &rasterizer));

  const std::string resource_name = "view_projection_matrix";
  const std::vector<float> resource_value(16);

  EXPECT_TRUE(
      rasterizer->SetUniformMatrix(resource_name, 4, 4, false, resource_value));
}

TEST(RasterizerTest, TestRender) {
  const std::vector<float> kViewProjectionMatrix = {
      -1.73205, 0.0, 0.0,     0.0, 0.0, 1.73205, 0.0,      0.0,
      0.0,      0.0, 1.22222, 1.0, 0.0, 0.0,     -2.22222, 0.0};
  std::unique_ptr<EGLOffscreenContext> context;
  std::unique_ptr<Rasterizer<float>> rasterizer;
  const int kWidth = 3;
  const int kHeight = 3;

  EXPECT_TRUE(EGLOffscreenContext::Create(&context));
  EXPECT_TRUE(context->MakeCurrent());
  EXPECT_TRUE(Rasterizer<float>::Create(kWidth, kHeight, empty_shader_code,
                                        geometry_shader_code,
                                        fragment_shader_code, &rasterizer));
  EXPECT_TRUE(rasterizer->SetUniformMatrix("view_projection_matrix", 4, 4,
                                           false, kViewProjectionMatrix));

  std::vector<float> rendering_result(kWidth * kHeight * 4);
  for (float depth = 2; depth < 5; ++depth) {
    std::vector<const float> geometry = {-10.0, 10.0, depth, 10.0, 10.0,
                                         depth, 0.0,  -10.0, depth};
    EXPECT_TRUE(rasterizer->SetShaderStorageBuffer("triangular_mesh",
                                                   absl::MakeSpan(geometry)));
    const int kNumVertices = geometry.size() / 3;
    EXPECT_TRUE(
        rasterizer->Render(kNumVertices, absl::MakeSpan(rendering_result)));

    for (int i = 0; i < kWidth * kHeight; ++i) {
      EXPECT_EQ(rendering_result[4 * i + 2], 0.0);
      EXPECT_EQ(rendering_result[4 * i + 3], depth);
    }
  }
}

}  // namespace
