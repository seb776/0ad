/* Copyright (C) 2021 Wildfire Games.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef INCLUDED_GLES2_WRAPPER
#define INCLUDED_GLES2_WRAPPER

#include "lib/config2.h" // CONFIG2_GLES

#if CONFIG2_GLES
#include <glad/gles2.h>

#define GL_FRAMEBUFFER_BINDING_EXT GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER
#define GL_WRITE_ONLY GL_WRITE_ONLY_OES
#define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0
#define GL_DEPTH_ATTACHMENT_EXT GL_DEPTH_ATTACHMENT

 // Functions

#define glActiveTextureARB glActiveTexture
#define glBlendColorEXT glBlendColor
#define glBlendEquationEXT glBlendEquation
#define glCompressedTexImage2DARB glCompressedTexImage2D
#define glAttachObjectARB glAttachShader
#define glBindAttribLocationARB glBindAttribLocation
#define glCompileShaderARB glCompileShader
#define glCreateProgramObjectARB glCreateProgram
#define glCreateShaderObjectARB glCreateShader
#define glDisableVertexAttribArrayARB glDisableVertexAttribArray
#define glEnableVertexAttribArrayARB glEnableVertexAttribArray
#define glGetActiveUniformARB glGetActiveUniform
#define glGetUniformLocationARB glGetUniformLocation
#define glLinkProgramARB glLinkProgram
#define glShaderSourceARB glShaderSource
#define glUniform1fARB glUniform1f
#define glUniform2fARB glUniform2f
#define glUniform3fARB glUniform3f
#define glUniform4fARB glUniform4f
#define glUniform1iARB glUniform1i
#define glUniform1fvARB glUniform1fv
#define glUniformMatrix4fvARB glUniformMatrix4fv
#define glUseProgramObjectARB glUseProgram
#define glVertexAttribPointerARB glVertexAttribPointer
#define glBindBufferARB glBindBuffer
#define glBufferDataARB glBufferData
#define glBufferSubDataARB glBufferSubData
#define glDeleteBuffersARB glDeleteBuffers
#define glGenBuffersARB glGenBuffers
#define glBindFramebufferEXT glBindFramebuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glGenFramebuffersEXT glGenFramebuffers

// Extensions

// GL_OES_texture_border_clamp
#define GL_CLAMP_TO_BORDER GL_CLAMP_TO_BORDER_OES
#define GL_TEXTURE_BORDER_COLOR GL_TEXTURE_BORDER_COLOR_OES

// GL_OES_rgb8_rgba8
#define GL_RGBA8 GL_RGBA8_OES

// GL_OES_mapbuffer
#define glMapBufferARB glMapBufferOES
#define glUnmapBufferARB glUnmapBufferOES

// GL_OES_depth32
#define GL_DEPTH_COMPONENT32 GL_DEPTH_COMPONENT32_OES

#endif // CONFIG2_GLES

#endif // !INCLUDED_GLES2_WRAPPER
