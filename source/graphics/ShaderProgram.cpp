/* Copyright (C) 2021 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "ShaderProgram.h"

#include "graphics/Color.h"
#include "graphics/PreprocessorWrapper.h"
#include "graphics/ShaderManager.h"
#include "graphics/TextureManager.h"
#include "lib/timer.h"
#include "lib/res/graphics/ogl_tex.h"
#include "maths/Matrix3D.h"
#include "maths/Vector3D.h"
#include "ps/CLogger.h"
#include "ps/Filesystem.h"

#include <algorithm>

#if !CONFIG2_GLES

class CShaderProgramARB : public CShaderProgram
{
public:
	CShaderProgramARB(const VfsPath& vertexFile, const VfsPath& fragmentFile,
		const CShaderDefines& defines,
		const std::map<CStrIntern, int>& vertexIndexes, const std::map<CStrIntern, frag_index_pair_t>& fragmentIndexes,
		int streamflags) :
		CShaderProgram(streamflags),
		m_VertexFile(vertexFile), m_FragmentFile(fragmentFile),
		m_Defines(defines),
		m_VertexIndexes(vertexIndexes), m_FragmentIndexes(fragmentIndexes)
	{
		glGenProgramsARB(1, &m_VertexProgram);
		glGenProgramsARB(1, &m_FragmentProgram);
	}

	~CShaderProgramARB()
	{
		Unload();

		glDeleteProgramsARB(1, &m_VertexProgram);
		glDeleteProgramsARB(1, &m_FragmentProgram);
	}

	bool Compile(GLuint target, const char* targetName, GLuint program, const VfsPath& file, const CStr& code)
	{
		ogl_WarnIfError();

		glBindProgramARB(target, program);

		ogl_WarnIfError();

		glProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)code.length(), code.c_str());

		if (ogl_SquelchError(GL_INVALID_OPERATION))
		{
			GLint errPos = 0;
			glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errPos);
			int errLine = std::count(code.begin(), code.begin() + std::min((int)code.length(), errPos + 1), '\n') + 1;
			char* errStr = (char*)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
			LOGERROR("Failed to compile %s program '%s' (line %d):\n%s", targetName, file.string8(), errLine, errStr);
			return false;
		}

		glBindProgramARB(target, 0);

		ogl_WarnIfError();

		return true;
	}

	void Reload() override
	{
		Unload();

		CVFSFile vertexFile;
		if (vertexFile.Load(g_VFS, m_VertexFile) != PSRETURN_OK)
			return;

		CVFSFile fragmentFile;
		if (fragmentFile.Load(g_VFS, m_FragmentFile) != PSRETURN_OK)
			return;

		CPreprocessorWrapper preprocessor;
		preprocessor.AddDefines(m_Defines);

		CStr vertexCode = preprocessor.Preprocess(vertexFile.GetAsString());
		CStr fragmentCode = preprocessor.Preprocess(fragmentFile.GetAsString());

		if (!Compile(GL_VERTEX_PROGRAM_ARB, "vertex", m_VertexProgram, m_VertexFile, vertexCode))
			return;

		if (!Compile(GL_FRAGMENT_PROGRAM_ARB, "fragment", m_FragmentProgram, m_FragmentFile, fragmentCode))
			return;

		m_IsValid = true;
	}

	void Unload()
	{
		m_IsValid = false;
	}

	void Bind() override
	{
		glEnable(GL_VERTEX_PROGRAM_ARB);
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_VertexProgram);
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_FragmentProgram);

		BindClientStates();
	}

	void Unbind() override
	{
		glDisable(GL_VERTEX_PROGRAM_ARB);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);

		UnbindClientStates();

		// TODO: should unbind textures, probably
	}

	int GetUniformVertexIndex(CStrIntern id)
	{
		std::map<CStrIntern, int>::iterator it = m_VertexIndexes.find(id);
		if (it == m_VertexIndexes.end())
			return -1;
		return it->second;
	}

	frag_index_pair_t GetUniformFragmentIndex(CStrIntern id)
	{
		std::map<CStrIntern, frag_index_pair_t>::iterator it = m_FragmentIndexes.find(id);
		if (it == m_FragmentIndexes.end())
			return std::make_pair(-1, 0);
		return it->second;
	}

	Binding GetTextureBinding(texture_id_t id) override
	{
		frag_index_pair_t fPair = GetUniformFragmentIndex(id);
		int index = fPair.first;
		if (index == -1)
			return Binding();
		else
			return Binding((int)fPair.second, index);
	}

	void BindTexture(texture_id_t id, GLuint tex) override
	{
		frag_index_pair_t fPair = GetUniformFragmentIndex(id);
		int index = fPair.first;
		if (index != -1)
		{
			glActiveTextureARB(GL_TEXTURE0 + index);
			glBindTexture(fPair.second, tex);
		}
	}

	void BindTexture(Binding id, GLuint tex) override
	{
		int index = id.second;
		if (index != -1)
		{
			glActiveTextureARB(GL_TEXTURE0 + index);
			glBindTexture(id.first, tex);
		}
	}

	Binding GetUniformBinding(uniform_id_t id) override
	{
		return Binding(GetUniformVertexIndex(id), GetUniformFragmentIndex(id).first);
	}

	void Uniform(Binding id, float v0, float v1, float v2, float v3) override
	{
		if (id.first != -1)
			glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, (GLuint)id.first, v0, v1, v2, v3);

		if (id.second != -1)
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, (GLuint)id.second, v0, v1, v2, v3);
	}

	void Uniform(Binding id, const CMatrix3D& v) override
	{
		if (id.first != -1)
		{
			glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, (GLuint)id.first+0, v._11, v._12, v._13, v._14);
			glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, (GLuint)id.first+1, v._21, v._22, v._23, v._24);
			glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, (GLuint)id.first+2, v._31, v._32, v._33, v._34);
			glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, (GLuint)id.first+3, v._41, v._42, v._43, v._44);
		}

		if (id.second != -1)
		{
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, (GLuint)id.second+0, v._11, v._12, v._13, v._14);
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, (GLuint)id.second+1, v._21, v._22, v._23, v._24);
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, (GLuint)id.second+2, v._31, v._32, v._33, v._34);
			glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, (GLuint)id.second+3, v._41, v._42, v._43, v._44);
		}
	}

	void Uniform(Binding id, size_t count, const CMatrix3D* v) override
	{
		ENSURE(count == 1);
		Uniform(id, v[0]);
	}

	void Uniform(Binding id, size_t count, const float* v) override
	{
		ENSURE(count == 4);
		Uniform(id, v[0], v[1], v[2], v[3]);
	}

	std::vector<VfsPath> GetFileDependencies() const override
	{
		return {m_VertexFile, m_FragmentFile};
	}

private:
	VfsPath m_VertexFile;
	VfsPath m_FragmentFile;
	CShaderDefines m_Defines;

	GLuint m_VertexProgram;
	GLuint m_FragmentProgram;

	std::map<CStrIntern, int> m_VertexIndexes;

	// pair contains <index, gltype>
	std::map<CStrIntern, frag_index_pair_t> m_FragmentIndexes;
};

#endif // #if !CONFIG2_GLES

//////////////////////////////////////////////////////////////////////////

TIMER_ADD_CLIENT(tc_ShaderGLSLCompile);
TIMER_ADD_CLIENT(tc_ShaderGLSLLink);

class CShaderProgramGLSL : public CShaderProgram
{
public:
	CShaderProgramGLSL(const VfsPath& vertexFile, const VfsPath& fragmentFile,
		const CShaderDefines& defines,
		const std::map<CStrIntern, int>& vertexAttribs,
		int streamflags) :
	CShaderProgram(streamflags),
		m_VertexFile(vertexFile), m_FragmentFile(fragmentFile),
		m_Defines(defines),
		m_VertexAttribs(vertexAttribs)
	{
		m_Program = 0;
		m_VertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER);
		m_FragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
		m_FileDependencies = {m_VertexFile, m_FragmentFile};
	}

	~CShaderProgramGLSL()
	{
		Unload();

		glDeleteShader(m_VertexShader);
		glDeleteShader(m_FragmentShader);
	}

	bool Compile(GLhandleARB shader, const VfsPath& file, const CStr& code)
	{
		TIMER_ACCRUE(tc_ShaderGLSLCompile);

		ogl_WarnIfError();

		const char* code_string = code.c_str();
		GLint code_length = code.length();
		glShaderSourceARB(shader, 1, &code_string, &code_length);

		glCompileShaderARB(shader);

		GLint ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

		GLint length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

		// Apparently sometimes GL_INFO_LOG_LENGTH is incorrectly reported as 0
		// (http://code.google.com/p/android/issues/detail?id=9953)
		if (!ok && length == 0)
			length = 4096;

		if (length > 1)
		{
			char* infolog = new char[length];
			glGetShaderInfoLog(shader, length, NULL, infolog);

			if (ok)
				LOGMESSAGE("Info when compiling shader '%s':\n%s", file.string8(), infolog);
			else
				LOGERROR("Failed to compile shader '%s':\n%s", file.string8(), infolog);

			delete[] infolog;
		}

		ogl_WarnIfError();

		return (ok ? true : false);
	}

	bool Link()
	{
		TIMER_ACCRUE(tc_ShaderGLSLLink);

		ENSURE(!m_Program);
		m_Program = glCreateProgramObjectARB();

		glAttachObjectARB(m_Program, m_VertexShader);
		ogl_WarnIfError();
		glAttachObjectARB(m_Program, m_FragmentShader);
		ogl_WarnIfError();

		// Set up the attribute bindings explicitly, since apparently drivers
		// don't always pick the most efficient bindings automatically,
		// and also this lets us hardcode indexes into VertexPointer etc
		for (std::map<CStrIntern, int>::iterator it = m_VertexAttribs.begin(); it != m_VertexAttribs.end(); ++it)
			glBindAttribLocationARB(m_Program, it->second, it->first.c_str());

		glLinkProgramARB(m_Program);

		GLint ok = 0;
		glGetProgramiv(m_Program, GL_LINK_STATUS, &ok);

		GLint length = 0;
		glGetProgramiv(m_Program, GL_INFO_LOG_LENGTH, &length);

		if (!ok && length == 0)
			length = 4096;

		if (length > 1)
		{
			char* infolog = new char[length];
			glGetProgramInfoLog(m_Program, length, NULL, infolog);

			if (ok)
				LOGMESSAGE("Info when linking program '%s'+'%s':\n%s", m_VertexFile.string8(), m_FragmentFile.string8(), infolog);
			else
				LOGERROR("Failed to link program '%s'+'%s':\n%s", m_VertexFile.string8(), m_FragmentFile.string8(), infolog);

			delete[] infolog;
		}

		ogl_WarnIfError();

		if (!ok)
			return false;

		m_Uniforms.clear();
		m_Samplers.clear();

		Bind();

		ogl_WarnIfError();

		GLint numUniforms = 0;
		glGetProgramiv(m_Program, GL_ACTIVE_UNIFORMS, &numUniforms);
		ogl_WarnIfError();
		for (GLint i = 0; i < numUniforms; ++i)
		{
			char name[256] = {0};
			GLsizei nameLength = 0;
			GLint size = 0;
			GLenum type = 0;
			glGetActiveUniformARB(m_Program, i, ARRAY_SIZE(name), &nameLength, &size, &type, name);
			ogl_WarnIfError();

			GLint loc = glGetUniformLocationARB(m_Program, name);

			CStrIntern nameIntern(name);
			m_Uniforms[nameIntern] = std::make_pair(loc, type);

			// Assign sampler uniforms to sequential texture units
			if (type == GL_SAMPLER_2D
			 || type == GL_SAMPLER_CUBE
#if !CONFIG2_GLES
			 || type == GL_SAMPLER_2D_SHADOW
#endif
			)
			{
				int unit = (int)m_Samplers.size();
				m_Samplers[nameIntern].first = (type == GL_SAMPLER_CUBE ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D);
				m_Samplers[nameIntern].second = unit;
				glUniform1iARB(loc, unit); // link uniform to unit
				ogl_WarnIfError();
			}
		}

		// TODO: verify that we're not using more samplers than is supported

		Unbind();

		ogl_WarnIfError();

		return true;
	}

	void Reload() override
	{
		Unload();

		CVFSFile vertexFile;
		if (vertexFile.Load(g_VFS, m_VertexFile) != PSRETURN_OK)
			return;

		CVFSFile fragmentFile;
		if (fragmentFile.Load(g_VFS, m_FragmentFile) != PSRETURN_OK)
			return;

		std::vector<VfsPath> newFileDependencies = {m_VertexFile, m_FragmentFile};

		CPreprocessorWrapper preprocessor([&newFileDependencies](const CStr& includePath, CStr& out) -> bool {
			const VfsPath includeFilePath(L"shaders/glsl/" + wstring_from_utf8(includePath));
			// Add dependencies anyway to reload the shader when the file is
			// appeared.
			newFileDependencies.push_back(includeFilePath);
			CVFSFile includeFile;
			if (includeFile.Load(g_VFS, includeFilePath) != PSRETURN_OK)
				return false;
			out = includeFile.GetAsString();
			return true;
		});
		preprocessor.AddDefines(m_Defines);

#if CONFIG2_GLES
		// GLES defines the macro "GL_ES" in its GLSL preprocessor,
		// but since we run our own preprocessor first, we need to explicitly
		// define it here
		preprocessor.AddDefine("GL_ES", "1");
#endif

		CStr vertexCode = preprocessor.Preprocess(vertexFile.GetAsString());
		CStr fragmentCode = preprocessor.Preprocess(fragmentFile.GetAsString());

		m_FileDependencies = std::move(newFileDependencies);

		if (vertexCode.empty())
			LOGERROR("Failed to preprocess vertex shader: '%s'", m_VertexFile.string8());
		if (fragmentCode.empty())
			LOGERROR("Failed to preprocess fragment shader: '%s'", m_FragmentFile.string8());

#if CONFIG2_GLES
		// Ugly hack to replace desktop GLSL 1.10/1.20 with GLSL ES 1.00,
		// and also to set default float precision for fragment shaders
		vertexCode.Replace("#version 110\n", "#version 100\n");
		vertexCode.Replace("#version 110\r\n", "#version 100\n");
		vertexCode.Replace("#version 120\n", "#version 100\n");
		vertexCode.Replace("#version 120\r\n", "#version 100\n");
		fragmentCode.Replace("#version 110\n", "#version 100\nprecision mediump float;\n");
		fragmentCode.Replace("#version 110\r\n", "#version 100\nprecision mediump float;\n");
		fragmentCode.Replace("#version 120\n", "#version 100\nprecision mediump float;\n");
		fragmentCode.Replace("#version 120\r\n", "#version 100\nprecision mediump float;\n");
#endif

		if (!Compile(m_VertexShader, m_VertexFile, vertexCode))
			return;

		if (!Compile(m_FragmentShader, m_FragmentFile, fragmentCode))
			return;

		if (!Link())
			return;

		m_IsValid = true;
	}

	void Unload()
	{
		m_IsValid = false;

		if (m_Program)
			glDeleteProgram(m_Program);
		m_Program = 0;

		// The shader objects can be reused and don't need to be deleted here
	}

	void Bind() override
	{
		glUseProgramObjectARB(m_Program);

		for (std::map<CStrIntern, int>::iterator it = m_VertexAttribs.begin(); it != m_VertexAttribs.end(); ++it)
			glEnableVertexAttribArrayARB(it->second);
	}

	void Unbind() override
	{
		glUseProgramObjectARB(0);

		for (std::map<CStrIntern, int>::iterator it = m_VertexAttribs.begin(); it != m_VertexAttribs.end(); ++it)
			glDisableVertexAttribArrayARB(it->second);

		// TODO: should unbind textures, probably
	}

	Binding GetTextureBinding(texture_id_t id) override
	{
		std::map<CStrIntern, std::pair<GLenum, int> >::iterator it = m_Samplers.find(CStrIntern(id));
		if (it == m_Samplers.end())
			return Binding();
		else
			return Binding((int)it->second.first, it->second.second);
	}

	void BindTexture(texture_id_t id, GLuint tex) override
	{
		std::map<CStrIntern, std::pair<GLenum, int> >::iterator it = m_Samplers.find(CStrIntern(id));
		if (it == m_Samplers.end())
			return;

		glActiveTextureARB(GL_TEXTURE0 + it->second.second);
		glBindTexture(it->second.first, tex);
	}

	void BindTexture(Binding id, GLuint tex) override
	{
		if (id.second == -1)
			return;

		glActiveTextureARB(GL_TEXTURE0 + id.second);
		glBindTexture(id.first, tex);
	}

	Binding GetUniformBinding(uniform_id_t id) override
	{
		std::map<CStrIntern, std::pair<int, GLenum> >::iterator it = m_Uniforms.find(id);
		if (it == m_Uniforms.end())
			return Binding();
		else
			return Binding(it->second.first, (int)it->second.second);
	}

	void Uniform(Binding id, float v0, float v1, float v2, float v3) override
	{
		if (id.first != -1)
		{
			if (id.second == GL_FLOAT)
				glUniform1fARB(id.first, v0);
			else if (id.second == GL_FLOAT_VEC2)
				glUniform2fARB(id.first, v0, v1);
			else if (id.second == GL_FLOAT_VEC3)
				glUniform3fARB(id.first, v0, v1, v2);
			else if (id.second == GL_FLOAT_VEC4)
				glUniform4fARB(id.first, v0, v1, v2, v3);
			else
				LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected float, vec2, vec3, vec4)");
		}
	}

	void Uniform(Binding id, const CMatrix3D& v) override
	{
		if (id.first != -1)
		{
			if (id.second == GL_FLOAT_MAT4)
				glUniformMatrix4fvARB(id.first, 1, GL_FALSE, &v._11);
			else
				LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected mat4)");
		}
	}

	void Uniform(Binding id, size_t count, const CMatrix3D* v) override
	{
		if (id.first != -1)
		{
			if (id.second == GL_FLOAT_MAT4)
				glUniformMatrix4fvARB(id.first, count, GL_FALSE, &v->_11);
			else
				LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected mat4)");
		}
	}

	void Uniform(Binding id, size_t count, const float* v) override
	{
		if (id.first != -1)
		{
			if (id.second == GL_FLOAT)
				glUniform1fvARB(id.first, count, v);
			else
				LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected float)");
		}
	}

	// Map the various fixed-function Pointer functions onto generic vertex attributes
	// (matching the attribute indexes from ShaderManager's ParseAttribSemantics):

	void VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) override
	{
		glVertexAttribPointerARB(0, size, type, GL_FALSE, stride, pointer);
		m_ValidStreams |= STREAM_POS;
	}

	void NormalPointer(GLenum type, GLsizei stride, const void* pointer) override
	{
		glVertexAttribPointerARB(2, 3, type, (type == GL_FLOAT ? GL_FALSE : GL_TRUE), stride, pointer);
		m_ValidStreams |= STREAM_NORMAL;
	}

	void ColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) override
	{
		glVertexAttribPointerARB(3, size, type, (type == GL_FLOAT ? GL_FALSE : GL_TRUE), stride, pointer);
		m_ValidStreams |= STREAM_COLOR;
	}

	void TexCoordPointer(GLenum texture, GLint size, GLenum type, GLsizei stride, const void* pointer) override
	{
		glVertexAttribPointerARB(8 + texture - GL_TEXTURE0, size, type, GL_FALSE, stride, pointer);
		m_ValidStreams |= STREAM_UV0 << (texture - GL_TEXTURE0);
	}

	void VertexAttribPointer(attrib_id_t id, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) override
	{
		std::map<CStrIntern, int>::iterator it = m_VertexAttribs.find(id);
		if (it != m_VertexAttribs.end())
		{
			glVertexAttribPointerARB(it->second, size, type, normalized, stride, pointer);
		}
	}

	void VertexAttribIPointer(attrib_id_t id, GLint size, GLenum type, GLsizei stride, const void* pointer) override
	{
		std::map<CStrIntern, int>::iterator it = m_VertexAttribs.find(id);
		if (it != m_VertexAttribs.end())
		{
#if CONFIG2_GLES
			debug_warn(L"glVertexAttribIPointer not supported on GLES");
#else
			glVertexAttribIPointerEXT(it->second, size, type, stride, pointer);
#endif
		}
	}

	std::vector<VfsPath> GetFileDependencies() const override
	{
		return m_FileDependencies;
	}

private:
	VfsPath m_VertexFile;
	VfsPath m_FragmentFile;
	std::vector<VfsPath> m_FileDependencies;
	CShaderDefines m_Defines;
	std::map<CStrIntern, int> m_VertexAttribs;

	GLhandleARB m_Program;
	GLhandleARB m_VertexShader;
	GLhandleARB m_FragmentShader;

	std::map<CStrIntern, std::pair<int, GLenum> > m_Uniforms;
	std::map<CStrIntern, std::pair<GLenum, int> > m_Samplers; // texture target & unit chosen for each uniform sampler
};

//////////////////////////////////////////////////////////////////////////

CShaderProgram::CShaderProgram(int streamflags)
	: m_IsValid(false), m_StreamFlags(streamflags), m_ValidStreams(0)
{
}

#if CONFIG2_GLES
/*static*/ CShaderProgram* CShaderProgram::ConstructARB(const VfsPath& vertexFile, const VfsPath& fragmentFile,
	const CShaderDefines& UNUSED(defines),
	const std::map<CStrIntern, int>& UNUSED(vertexIndexes), const std::map<CStrIntern, frag_index_pair_t>& UNUSED(fragmentIndexes),
	int UNUSED(streamflags))
{
	LOGERROR("CShaderProgram::ConstructARB: '%s'+'%s': ARB shaders not supported on this device",
		vertexFile.string8(), fragmentFile.string8());
	return NULL;
}
#else
/*static*/ CShaderProgram* CShaderProgram::ConstructARB(const VfsPath& vertexFile, const VfsPath& fragmentFile,
	const CShaderDefines& defines,
	const std::map<CStrIntern, int>& vertexIndexes, const std::map<CStrIntern, frag_index_pair_t>& fragmentIndexes,
	int streamflags)
{
	return new CShaderProgramARB(vertexFile, fragmentFile, defines, vertexIndexes, fragmentIndexes, streamflags);
}
#endif

/*static*/ CShaderProgram* CShaderProgram::ConstructGLSL(const VfsPath& vertexFile, const VfsPath& fragmentFile,
	const CShaderDefines& defines,
	const std::map<CStrIntern, int>& vertexAttribs,
	int streamflags)
{
	return new CShaderProgramGLSL(vertexFile, fragmentFile, defines, vertexAttribs, streamflags);
}

bool CShaderProgram::IsValid() const
{
	return m_IsValid;
}

int CShaderProgram::GetStreamFlags() const
{
	return m_StreamFlags;
}

void CShaderProgram::BindTexture(texture_id_t id, const CTexturePtr& tex)
{
	GLuint h;
	ogl_tex_get_texture_id(tex->GetHandle(), &h);
	BindTexture(id, h);
}

void CShaderProgram::BindTexture(Binding id, const CTexturePtr& tex)
{
	GLuint h;
	ogl_tex_get_texture_id(tex->GetHandle(), &h);
	BindTexture(id, h);
}

void CShaderProgram::Uniform(Binding id, int v)
{
	Uniform(id, (float)v, (float)v, (float)v, (float)v);
}

void CShaderProgram::Uniform(Binding id, float v)
{
	Uniform(id, v, v, v, v);
}

void CShaderProgram::Uniform(Binding id, float v0, float v1)
{
	Uniform(id, v0, v1, 0.0f, 0.0f);
}

void CShaderProgram::Uniform(Binding id, const CVector3D& v)
{
	Uniform(id, v.X, v.Y, v.Z, 0.0f);
}

void CShaderProgram::Uniform(Binding id, const CColor& v)
{
	Uniform(id, v.r, v.g, v.b, v.a);
}

void CShaderProgram::Uniform(uniform_id_t id, int v)
{
	Uniform(GetUniformBinding(id), (float)v, (float)v, (float)v, (float)v);
}

void CShaderProgram::Uniform(uniform_id_t id, float v)
{
	Uniform(GetUniformBinding(id), v, v, v, v);
}

void CShaderProgram::Uniform(uniform_id_t id, float v0, float v1)
{
	Uniform(GetUniformBinding(id), v0, v1, 0.0f, 0.0f);
}

void CShaderProgram::Uniform(uniform_id_t id, const CVector3D& v)
{
	Uniform(GetUniformBinding(id), v.X, v.Y, v.Z, 0.0f);
}

void CShaderProgram::Uniform(uniform_id_t id, const CColor& v)
{
	Uniform(GetUniformBinding(id), v.r, v.g, v.b, v.a);
}

void CShaderProgram::Uniform(uniform_id_t id, float v0, float v1, float v2, float v3)
{
	Uniform(GetUniformBinding(id), v0, v1, v2, v3);
}

void CShaderProgram::Uniform(uniform_id_t id, const CMatrix3D& v)
{
	Uniform(GetUniformBinding(id), v);
}

void CShaderProgram::Uniform(uniform_id_t id, size_t count, const CMatrix3D* v)
{
	Uniform(GetUniformBinding(id), count, v);
}

void CShaderProgram::Uniform(uniform_id_t id, size_t count, const float* v)
{
	Uniform(GetUniformBinding(id), count, v);
}


// These should all be overridden by CShaderProgramGLSL, and not used
// if a non-GLSL shader was loaded instead:

void CShaderProgram::VertexAttribPointer(attrib_id_t UNUSED(id), GLint UNUSED(size), GLenum UNUSED(type),
	GLboolean UNUSED(normalized), GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("Shader type doesn't support VertexAttribPointer");
}

void CShaderProgram::VertexAttribIPointer(attrib_id_t UNUSED(id), GLint UNUSED(size), GLenum UNUSED(type),
	GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("Shader type doesn't support VertexAttribIPointer");
}

#if CONFIG2_GLES

// These should all be overridden by CShaderProgramGLSL
// (GLES doesn't support any other types of shader program):

void CShaderProgram::VertexPointer(GLint UNUSED(size), GLenum UNUSED(type), GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("CShaderProgram::VertexPointer should be overridden");
}
void CShaderProgram::NormalPointer(GLenum UNUSED(type), GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("CShaderProgram::NormalPointer should be overridden");
}
void CShaderProgram::ColorPointer(GLint UNUSED(size), GLenum UNUSED(type), GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("CShaderProgram::ColorPointer should be overridden");
}
void CShaderProgram::TexCoordPointer(GLenum UNUSED(texture), GLint UNUSED(size), GLenum UNUSED(type), GLsizei UNUSED(stride), const void* UNUSED(pointer))
{
	debug_warn("CShaderProgram::TexCoordPointer should be overridden");
}

#else

// These are overridden by CShaderProgramGLSL, but fixed-function and ARB shaders
// both use the fixed-function vertex attribute pointers so we'll share their
// definitions here:

void CShaderProgram::VertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
	glVertexPointer(size, type, stride, pointer);
	m_ValidStreams |= STREAM_POS;
}

void CShaderProgram::NormalPointer(GLenum type, GLsizei stride, const void* pointer)
{
	glNormalPointer(type, stride, pointer);
	m_ValidStreams |= STREAM_NORMAL;
}

void CShaderProgram::ColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer)
{
	glColorPointer(size, type, stride, pointer);
	m_ValidStreams |= STREAM_COLOR;
}

void CShaderProgram::TexCoordPointer(GLenum texture, GLint size, GLenum type, GLsizei stride, const void* pointer)
{
	glClientActiveTextureARB(texture);
	glTexCoordPointer(size, type, stride, pointer);
	glClientActiveTextureARB(GL_TEXTURE0);
	m_ValidStreams |= STREAM_UV0 << (texture - GL_TEXTURE0);
}

void CShaderProgram::BindClientStates()
{
	ENSURE(m_StreamFlags == (m_StreamFlags & (STREAM_POS|STREAM_NORMAL|STREAM_COLOR|STREAM_UV0|STREAM_UV1)));

	// Enable all the desired client states for non-GLSL rendering

	if (m_StreamFlags & STREAM_POS)    glEnableClientState(GL_VERTEX_ARRAY);
	if (m_StreamFlags & STREAM_NORMAL) glEnableClientState(GL_NORMAL_ARRAY);
	if (m_StreamFlags & STREAM_COLOR)  glEnableClientState(GL_COLOR_ARRAY);

	if (m_StreamFlags & STREAM_UV0)
	{
		glClientActiveTextureARB(GL_TEXTURE0);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (m_StreamFlags & STREAM_UV1)
	{
		glClientActiveTextureARB(GL_TEXTURE1);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTextureARB(GL_TEXTURE0);
	}

	// Rendering code must subsequently call VertexPointer etc for all of the streams
	// that were activated in this function, else AssertPointersBound will complain
	// that some arrays were unspecified
	m_ValidStreams = 0;
}

void CShaderProgram::UnbindClientStates()
{
	if (m_StreamFlags & STREAM_POS)    glDisableClientState(GL_VERTEX_ARRAY);
	if (m_StreamFlags & STREAM_NORMAL) glDisableClientState(GL_NORMAL_ARRAY);
	if (m_StreamFlags & STREAM_COLOR)  glDisableClientState(GL_COLOR_ARRAY);

	if (m_StreamFlags & STREAM_UV0)
	{
		glClientActiveTextureARB(GL_TEXTURE0);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (m_StreamFlags & STREAM_UV1)
	{
		glClientActiveTextureARB(GL_TEXTURE1);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTextureARB(GL_TEXTURE0);
	}
}

#endif // !CONFIG2_GLES

void CShaderProgram::AssertPointersBound()
{
	ENSURE((m_StreamFlags & ~m_ValidStreams) == 0);
}
