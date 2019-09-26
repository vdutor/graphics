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
#include "tensorflow_graphics/rendering/opengl/gl_utils.h"

#include <memory>
#include <string>

#include "absl/strings/string_view.h"

namespace gl_utils {

Program::Program(GLuint program_handle) : program_handle_(program_handle) {}

Program::~Program() { glDeleteProgram(program_handle_); }

tensorflow::Status Program::CompileShader(const string& shader_code,
                                          const GLenum& shader_type,
                                          GLuint* shader_idx) {
  // Create an empty shader object.
  RETURN_IF_GL_ERROR(*shader_idx = glCreateShader(shader_type));
  if (*shader_idx == 0)
    return INVALID_ARGUMENT("Error while creating the shader object.");
  auto shader_cleanup =
      MakeCleanup([shader_idx]() { glDeleteShader(*shader_idx); });

  // Set the source code in the shader object.
  auto shader_code_c_str = shader_code.c_str();
  RETURN_IF_GL_ERROR(
      glShaderSource(*shader_idx, 1, &shader_code_c_str, nullptr));

  // Compile the shader.
  RETURN_IF_GL_ERROR(glCompileShader(*shader_idx));

  GLint compilation_status;
  RETURN_IF_GL_ERROR(
      glGetShaderiv(*shader_idx, GL_COMPILE_STATUS, &compilation_status));
  if (compilation_status != GL_TRUE) {
    GLsizei log_length;
    RETURN_IF_GL_ERROR(
        glGetShaderiv(*shader_idx, GL_INFO_LOG_LENGTH, &log_length));

    std::vector<char> info_log(log_length + 1);
    RETURN_IF_GL_ERROR(
        glGetShaderInfoLog(*shader_idx, log_length, nullptr, &info_log[0]));
    RETURN_IF_GL_ERROR(glDeleteShader(*shader_idx));

    std::string error_message =
        "Error while compiling the shader: " + std::string(&info_log[0]);
    return INVALID_ARGUMENT(error_message);
  }
  shader_cleanup.release();
  return tensorflow::Status::OK();
}

tensorflow::Status Program::Create(
    const std::vector<std::pair<std::string, GLenum>>& shaders,
    std::unique_ptr<Program>* program) {
  // Create an empty program object.
  GLuint program_handle;

  program_handle = glCreateProgram();
  if (program_handle == 0)
    return INVALID_ARGUMENT("Error while creating the program object.");
  auto program_cleanup =
      MakeCleanup([program_handle]() { glDeleteProgram(program_handle); });

  // Compile and attach the input shaders to the program.
  std::vector<Cleanup<std::function<void()>>> shader_cleanups;
  for (auto shader : shaders) {
    GLuint shader_idx;
    RETURN_IF_TF_ERROR(CompileShader(shader.first, shader.second, &shader_idx));
    std::function<void()> compile_cleanup = [shader_idx]() {
      glDeleteShader(shader_idx);
    };
    shader_cleanups.push_back(MakeCleanup(compile_cleanup));

    RETURN_IF_GL_ERROR(glAttachShader(program_handle, shader_idx));
    std::function<void()> attach_cleanup = [program_handle, shader_idx]() {
      glDetachShader(program_handle, shader_idx);
    };
    shader_cleanups.push_back(MakeCleanup(attach_cleanup));
  }

  // Link the program to the executable that will run on the programmable
  // vertex/fragment processors.
  RETURN_IF_GL_ERROR(glLinkProgram(program_handle));
  *program = std::unique_ptr<Program>(new Program(program_handle));

  program_cleanup.release();
  // The content of shader_cleanups needs cleanup and hence is not released; see
  // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDeleteProgram.xhtml.
  return tensorflow::Status::OK();
}

tensorflow::Status Program::GetProgramResourceIndex(
    GLenum program_interface, absl::string_view resource_name,
    GLuint* resource_index) const {
  RETURN_IF_GL_ERROR(*resource_index = glGetProgramResourceIndex(
                         program_handle_, program_interface,
                         resource_name.data()));
  return tensorflow::Status::OK();
}

tensorflow::Status Program::GetProgramResourceiv(
    GLenum program_interface, GLuint resource_index, int num_properties,
    const GLenum* properties, int num_property_value, GLsizei* length,
    GLint* property_value) const {
  RETURN_IF_GL_ERROR(glGetProgramResourceiv(
      program_handle_, program_interface, resource_index, num_properties,
      properties, num_property_value, length, property_value));
  return tensorflow::Status::OK();
}

tensorflow::Status Program::GetResourceProperty(
    const std::string& resource_name, GLenum program_interface,
    int num_properties, const GLenum* properties, int num_property_value,
    GLint* property_value) {
  if (num_property_value != num_properties)
    return INVALID_ARGUMENT("num_property_value != num_properties");

  GLuint resource_index;
  // Query the index of the named resource within the program.
  RETURN_IF_TF_ERROR(GetProgramResourceIndex(program_interface, resource_name,
                                             &resource_index));

  // No resource is active under that name.
  if (resource_index == GL_INVALID_INDEX)
    return INVALID_ARGUMENT("GL_INVALID_INDEX");

  // Retrieve the value for the property.
  GLsizei length;
  RETURN_IF_TF_ERROR(GetProgramResourceiv(
      program_interface, resource_index, num_properties, properties,
      num_property_value, &length, property_value));

  if (length != num_properties)
    return INVALID_ARGUMENT(
        "length != num_properties: " + std::to_string(length) +
        " != " + std::to_string(num_properties));

  return tensorflow::Status::OK();
}

tensorflow::Status Program::Use() const {
  RETURN_IF_GL_ERROR(glUseProgram(program_handle_));
  return tensorflow::Status::OK();
}

ShaderStorageBuffer::ShaderStorageBuffer(GLuint buffer) : buffer_(buffer) {}

ShaderStorageBuffer::~ShaderStorageBuffer() { glDeleteBuffers(1, &buffer_); }

tensorflow::Status ShaderStorageBuffer::Create(
    std::unique_ptr<ShaderStorageBuffer>* shader_storage_buffer) {
  GLuint buffer;

  // Generate one buffer object.
  RETURN_IF_GL_ERROR(glGenBuffers(1, &buffer));
  *shader_storage_buffer =
      std::unique_ptr<ShaderStorageBuffer>(new ShaderStorageBuffer(buffer));
  return tensorflow::Status::OK();
}

tensorflow::Status ShaderStorageBuffer::BindBufferBase(GLuint index) const {
  RETURN_IF_GL_ERROR(
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, buffer_));
  return tensorflow::Status::OK();
}

}  // namespace gl_utils
