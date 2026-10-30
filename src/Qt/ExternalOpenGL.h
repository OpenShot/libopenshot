/*
** Copyright (c) 2013-2018 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/
// Subset of Khronos OpenGL-Registry api/GL/glext.h for Qt 5 SDKs whose
// extension headers predate external memory and semaphore interop.
#ifndef OPENSHOT_EXTERNAL_OPENGL_H
#define OPENSHOT_EXTERNAL_OPENGL_H
#include <QtGui/qopenglext.h>

#ifndef GL_EXT_memory_object
#define GL_EXT_memory_object 1
#define GL_TEXTURE_TILING_EXT             0x9580
#define GL_DEDICATED_MEMORY_OBJECT_EXT    0x9581
#define GL_PROTECTED_MEMORY_OBJECT_EXT    0x959B
#define GL_NUM_TILING_TYPES_EXT           0x9582
#define GL_TILING_TYPES_EXT               0x9583
#define GL_OPTIMAL_TILING_EXT             0x9584
#define GL_LINEAR_TILING_EXT              0x9585
#define GL_NUM_DEVICE_UUIDS_EXT           0x9596
#define GL_DEVICE_UUID_EXT                0x9597
#define GL_DRIVER_UUID_EXT                0x9598
#define GL_UUID_SIZE_EXT                  16
typedef void (APIENTRYP PFNGLGETUNSIGNEDBYTEVEXTPROC) (GLenum pname, GLubyte *data);
typedef void (APIENTRYP PFNGLGETUNSIGNEDBYTEI_VEXTPROC) (GLenum target, GLuint index, GLubyte *data);
typedef void (APIENTRYP PFNGLDELETEMEMORYOBJECTSEXTPROC) (GLsizei n, const GLuint *memoryObjects);
typedef GLboolean (APIENTRYP PFNGLISMEMORYOBJECTEXTPROC) (GLuint memoryObject);
typedef void (APIENTRYP PFNGLCREATEMEMORYOBJECTSEXTPROC) (GLsizei n, GLuint *memoryObjects);
typedef void (APIENTRYP PFNGLMEMORYOBJECTPARAMETERIVEXTPROC) (GLuint memoryObject, GLenum pname, const GLint *params);
typedef void (APIENTRYP PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC) (GLuint memoryObject, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM2DEXTPROC) (GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM2DMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM3DEXTPROC) (GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM3DMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLBUFFERSTORAGEMEMEXTPROC) (GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM2DEXTPROC) (GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC) (GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM3DEXTPROC) (GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC) (GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLNAMEDBUFFERSTORAGEMEMEXTPROC) (GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM1DEXTPROC) (GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM1DEXTPROC) (GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLuint memory, GLuint64 offset);
#endif /* GL_EXT_memory_object */

#ifndef GL_EXT_memory_object_fd
#define GL_EXT_memory_object_fd 1
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT      0x9586
typedef void (APIENTRYP PFNGLIMPORTMEMORYFDEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
#endif /* GL_EXT_memory_object_fd */

#ifndef GL_EXT_memory_object_win32
#define GL_EXT_memory_object_win32 1
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT   0x9587
#define GL_HANDLE_TYPE_OPAQUE_WIN32_KMT_EXT 0x9588
#define GL_DEVICE_LUID_EXT                0x9599
#define GL_DEVICE_NODE_MASK_EXT           0x959A
#define GL_LUID_SIZE_EXT                  8
#define GL_HANDLE_TYPE_D3D12_TILEPOOL_EXT 0x9589
#define GL_HANDLE_TYPE_D3D12_RESOURCE_EXT 0x958A
#define GL_HANDLE_TYPE_D3D11_IMAGE_EXT    0x958B
#define GL_HANDLE_TYPE_D3D11_IMAGE_KMT_EXT 0x958C
typedef void (APIENTRYP PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, void *handle);
typedef void (APIENTRYP PFNGLIMPORTMEMORYWIN32NAMEEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, const void *name);
#endif /* GL_EXT_memory_object_win32 */

#ifndef GL_EXT_semaphore
#define GL_EXT_semaphore 1
#define GL_LAYOUT_GENERAL_EXT             0x958D
#define GL_LAYOUT_COLOR_ATTACHMENT_EXT    0x958E
#define GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT 0x958F
#define GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT 0x9590
#define GL_LAYOUT_SHADER_READ_ONLY_EXT    0x9591
#define GL_LAYOUT_TRANSFER_SRC_EXT        0x9592
#define GL_LAYOUT_TRANSFER_DST_EXT        0x9593
#define GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT 0x9530
#define GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT 0x9531
typedef void (APIENTRYP PFNGLGENSEMAPHORESEXTPROC) (GLsizei n, GLuint *semaphores);
typedef void (APIENTRYP PFNGLDELETESEMAPHORESEXTPROC) (GLsizei n, const GLuint *semaphores);
typedef GLboolean (APIENTRYP PFNGLISSEMAPHOREEXTPROC) (GLuint semaphore);
typedef void (APIENTRYP PFNGLSEMAPHOREPARAMETERUI64VEXTPROC) (GLuint semaphore, GLenum pname, const GLuint64 *params);
typedef void (APIENTRYP PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC) (GLuint semaphore, GLenum pname, GLuint64 *params);
typedef void (APIENTRYP PFNGLWAITSEMAPHOREEXTPROC) (GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts);
typedef void (APIENTRYP PFNGLSIGNALSEMAPHOREEXTPROC) (GLuint semaphore, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *dstLayouts);
#endif /* GL_EXT_semaphore */

#ifndef GL_EXT_semaphore_fd
#define GL_EXT_semaphore_fd 1
typedef void (APIENTRYP PFNGLIMPORTSEMAPHOREFDEXTPROC) (GLuint semaphore, GLenum handleType, GLint fd);
#endif /* GL_EXT_semaphore_fd */

#ifndef GL_EXT_semaphore_win32
#define GL_EXT_semaphore_win32 1
#define GL_HANDLE_TYPE_D3D12_FENCE_EXT    0x9594
#define GL_D3D12_FENCE_VALUE_EXT          0x9595
typedef void (APIENTRYP PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC) (GLuint semaphore, GLenum handleType, void *handle);
typedef void (APIENTRYP PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC) (GLuint semaphore, GLenum handleType, const void *name);
#endif /* GL_EXT_semaphore_win32 */
#endif
